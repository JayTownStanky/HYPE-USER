#include "game_sdk.h"
#include "offsets.hpp"
#include <spdlog/spdlog.h>
#include <Windows.h>
#include <TlHelp32.h>

static uint32_t find_process_id(const std::string& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            // Convert wide to narrow for comparison
            char narrow[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, narrow, MAX_PATH, nullptr, nullptr);
            if (_stricmp(narrow, name.c_str()) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

static uint64_t get_system_eprocess() {
    // System process EPROCESS - we need kernel access for this
    // For now, use a known method via NtQuerySystemInformation
    // This will be provided by the hypervisor's cached EPROCESS
    return 0;  // Caller should provide or hypervisor returns it
}

bool GameState::init(HvMemory& mem, const Config& cfg) {
    m_mem = &mem;
    m_cfg = &cfg;
    m_ready = false;

    // Find process
    uint32_t pid = find_process_id(cfg.game.process_name);
    if (pid == 0) {
        spdlog::error("Process not found: {}", cfg.game.process_name);
        return false;
    }

    spdlog::info("Found process {} with PID {}", cfg.game.process_name, pid);

    // Get EPROCESS from hypervisor (it knows System EPROCESS)
    // Use PING to get EPROCESS base, or use a separate VMMCALL
    uint64_t eprocess = hv::call(hv::CMD_VERSION, 0, 0, 0, 0);  // Returns EPROCESS base
    if (eprocess == 0 || eprocess == ~0ULL) {
        // Try without EPROCESS - hypervisor may have it cached
        if (!mem.attach(pid, 0)) {
            spdlog::error("Failed to attach to process");
            return false;
        }
    } else {
        if (!mem.attach(pid, eprocess)) {
            spdlog::error("Failed to attach to process with EPROCESS");
            return false;
        }
    }

    // Get module bases
    m_client_base = mem.get_module_base("client.dll");
    m_engine_base = mem.get_module_base("engine.dll");

    if (m_client_base == 0) {
        spdlog::error("Failed to find client.dll");
        return false;
    }

    spdlog::info("client.dll @ 0x{:X}", m_client_base);
    if (m_engine_base != 0) {
        spdlog::info("engine.dll @ 0x{:X}", m_engine_base);
    }

    // Cache global offsets from config
    m_off_entity_list = get_offset(cfg, "entity_list");
    m_off_local_player = get_offset(cfg, "local_player");
    m_off_view_matrix = get_offset(cfg, "view_matrix");
    m_off_name_list = get_offset(cfg, "name_list");
    m_off_level = get_offset(cfg, "level");
    m_off_glow_highlights = get_offset(cfg, "glow_highlights");

    m_ready = true;
    spdlog::info("GameState initialized");
    return true;
}

bool GameState::update() {
    if (!m_ready || !m_mem) {
        return false;
    }

    if (!read_local_player()) {
        return false;
    }

    if (!read_entities()) {
        return false;
    }

    if (!read_view_matrix()) {
        return false;
    }

    return true;
}

bool GameState::read_local_player() {
    if (m_off_local_player == 0) {
        return false;
    }

    auto local_ptr = m_mem->read<uint64_t>(m_client_base + m_off_local_player);
    if (!local_ptr || *local_ptr == 0) {
        return false;
    }

    m_local = read_entity(*local_ptr);
    return m_local.valid();
}

bool GameState::read_entities() {
    m_entities.clear();

    if (m_off_entity_list == 0) {
        return false;
    }

    // Read entity list (game-specific structure)
    uint64_t list_base = m_client_base + m_off_entity_list;

    // Typical entity list: array of pointers, 64 max
    constexpr int MAX_ENTITIES = 64;

    for (int i = 0; i < MAX_ENTITIES; i++) {
        auto ent_ptr = m_mem->read<uint64_t>(list_base + i * sizeof(uint64_t));
        if (!ent_ptr || *ent_ptr == 0) {
            continue;
        }

        Entity ent = read_entity(*ent_ptr);
        if (ent.valid()) {
            m_entities.push_back(ent);
        }
    }

    return true;
}

bool GameState::read_view_matrix() {
    if (m_off_view_matrix == 0) {
        return false;
    }

    uint64_t matrix_addr = m_client_base + m_off_view_matrix;
    return m_mem->read_raw(matrix_addr, &m_view_matrix, sizeof(ViewMatrix));
}

Entity GameState::read_entity(uint64_t address) {
    Entity ent;
    ent.address = address;

    // Defensive check - should never happen but prevents crash
    if (!m_mem) {
        return ent;  // Return invalid/empty entity
    }

    // Read health (struct-relative offset from offsets.hpp)
    auto health = m_mem->read<int32_t>(address + OFF_HEALTH);
    if (health) ent.health = *health;

    auto max_health = m_mem->read<int32_t>(address + OFF_MAX_HEALTH);
    if (max_health) ent.max_health = *max_health;

    // Read team
    auto team = m_mem->read<int32_t>(address + OFF_TEAM_NUMBER);
    if (team) ent.team = *team;

    // Read position
    auto pos = m_mem->read<Vec3>(address + OFF_ABS_ORIGIN);
    if (pos) ent.position = *pos;

    // Read life state as dormant proxy
    auto life_state = m_mem->read<int16_t>(address + OFF_LIFE_STATE);
    if (life_state) ent.dormant = (*life_state != 0);

    // Read head position from bone matrix
    auto bone_ptr = m_mem->read<uint64_t>(address + OFF_BONES);
    if (bone_ptr && *bone_ptr != 0) {
        // Head bone is typically index 8, each bone is 0x30 bytes (3x4 matrix)
        constexpr int HEAD_BONE = 8;
        constexpr int BONE_SIZE = 0x30;

        uint64_t head_addr = *bone_ptr + HEAD_BONE * BONE_SIZE;
        auto head_x = m_mem->read<float>(head_addr + 0x0C);
        auto head_y = m_mem->read<float>(head_addr + 0x1C);
        auto head_z = m_mem->read<float>(head_addr + 0x2C);

        if (head_x && head_y && head_z) {
            ent.head_position = Vec3(*head_x, *head_y, *head_z);
        }
    }

    return ent;
}
