#include "mem.h"
#include <cstring>
#include <cctype>
#include <windows.h>
#include <spdlog/spdlog.h>
#include <tlhelp32.h>

bool HvMemory::attach(uint32_t pid, uint64_t eprocess_va) {
    m_cr3 = 0;
    m_eprocess = 0;
    m_pid = 0;
    m_va_cache.clear();

    // Translate EPROCESS VA to PA (using kernel CR3 or current)
    // For initial attach, hypervisor uses its own translation
    uint64_t eprocess_pa = hv::call(hv::CMD_TRANSLATE_VA, 0, eprocess_va, 0, 0);
    if (eprocess_pa == 0 || eprocess_pa == ~0ULL) {
        return false;
    }

    // Set EPROCESS list head for CR3 lookups
    uint64_t status = hv::call(hv::CMD_SET_EPROCESS, eprocess_pa, 0, 0, 0);
    if (status != 0) {
        return false;
    }

    // Get CR3 for target PID
    // Pass offsets: p1=PID, p2=packed offsets (links|pid|dtb)
    uint64_t packed_offsets =
        (OFF_ACTIVE_PROCESS_LINKS) |
        (OFF_UNIQUE_PROCESS_ID << 16) |
        (OFF_DIRECTORY_TABLE_BASE << 32);

    uint64_t cr3 = hv::call(hv::CMD_GET_CR3, pid, packed_offsets, 0, 0);
    if (cr3 == 0 || cr3 == ~0ULL) {
        return false;
    }

    m_cr3 = cr3;
    m_eprocess = eprocess_pa;
    m_pid = pid;

    // Initialize shared page for scatter operations
    if (!init_shared_page()) {
        spdlog::warn("Failed to initialize shared page (scatter operations unavailable)");
    }

    return true;
}

bool HvMemory::init_shared_page() {
    if (m_shared_page_initialized) {
        return true;
    }

    // Allocate 4KB page
    m_shared_page_va = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!m_shared_page_va) {
        spdlog::error("Failed to allocate shared page: {}", GetLastError());
        return false;
    }

    // Get our own PID
    uint32_t our_pid = GetCurrentProcessId();

    // Get our own CR3 (need system EPROCESS VA which should already be set)
    uint64_t our_cr3 = hv::call(hv::CMD_GET_CR3, our_pid,
        (OFF_ACTIVE_PROCESS_LINKS) |
        (OFF_UNIQUE_PROCESS_ID << 16) |
        (OFF_DIRECTORY_TABLE_BASE << 32), 0, 0);

    if (our_cr3 == 0 || our_cr3 == ~0ULL) {
        spdlog::error("Failed to get own CR3 for shared page translation");
        VirtualFree(m_shared_page_va, 0, MEM_RELEASE);
        m_shared_page_va = nullptr;
        return false;
    }

    // Translate shared page VA to GPA
    m_shared_page_gpa = hv::call(hv::CMD_TRANSLATE_VA, our_cr3,
        reinterpret_cast<uint64_t>(m_shared_page_va), 0, 0);

    if (m_shared_page_gpa == 0 || m_shared_page_gpa == ~0ULL) {
        spdlog::error("Failed to translate shared page VA to GPA");
        VirtualFree(m_shared_page_va, 0, MEM_RELEASE);
        m_shared_page_va = nullptr;
        return false;
    }

    // Register with hypervisor
    uint64_t status = hv::call(hv::CMD_SHARED_INIT, m_shared_page_gpa, 0, 0, 0);
    if (status != 0) {
        spdlog::error("Hypervisor rejected shared page initialization: 0x{:X}", status);
        VirtualFree(m_shared_page_va, 0, MEM_RELEASE);
        m_shared_page_va = nullptr;
        m_shared_page_gpa = 0;
        return false;
    }

    m_shared_page_initialized = true;
    spdlog::info("Shared page initialized: VA=0x{:X}, GPA=0x{:X}",
        reinterpret_cast<uint64_t>(m_shared_page_va), m_shared_page_gpa);

    return true;
}

uint64_t HvMemory::get_module_base(const char* name) {
    if (!attached()) {
        return 0;
    }

    uint64_t hash = djb2_hash(name);
    return hv::call(hv::CMD_GET_MODULE_BASE, m_cr3, hash, 0, 0);
}

bool HvMemory::read_raw(uint64_t va, void* buf, size_t size) {
    if (!attached() || !buf || size == 0) {
        return false;
    }

    uint8_t* dst = static_cast<uint8_t*>(buf);
    size_t remaining = size;
    uint64_t current_va = va;

    while (remaining > 0) {
        // Translate current VA
        uint64_t pa = translate(current_va);
        if (pa == 0 || pa == ~0ULL) {
            return false;
        }

        // Read up to page boundary
        size_t page_offset = current_va & 0xFFF;
        size_t chunk = 0x1000 - page_offset;
        if (chunk > remaining) {
            chunk = remaining;
        }

        uint64_t status = hv::call(hv::CMD_READ_PHYS, pa, chunk,
                                   reinterpret_cast<uint64_t>(dst), 0);
        if (status != 0) {
            return false;
        }

        dst += chunk;
        current_va += chunk;
        remaining -= chunk;
    }

    return true;
}

bool HvMemory::write_raw(uint64_t va, const void* buf, size_t size) {
    if (!attached() || !buf || size == 0) {
        return false;
    }

    const uint8_t* src = static_cast<const uint8_t*>(buf);
    size_t remaining = size;
    uint64_t current_va = va;

    while (remaining > 0) {
        uint64_t pa = translate(current_va);
        if (pa == 0 || pa == ~0ULL) {
            return false;
        }

        size_t page_offset = current_va & 0xFFF;
        size_t chunk = 0x1000 - page_offset;
        if (chunk > remaining) {
            chunk = remaining;
        }

        uint64_t status = hv::call(hv::CMD_WRITE_PHYS, pa, chunk,
                                   reinterpret_cast<uint64_t>(src), 0);
        if (status != 0) {
            return false;
        }

        src += chunk;
        current_va += chunk;
        remaining -= chunk;
    }

    return true;
}

bool HvMemory::scatter_read(std::vector<ScatterEntry>& entries) {
    if (!attached() || entries.empty()) {
        return false;
    }

    // Initialize shared page if needed
    if (!m_shared_page_initialized && !init_shared_page()) {
        spdlog::warn("Shared page unavailable, falling back to individual reads");
        for (auto& entry : entries) {
            if (!read_raw(entry.va, entry.output_buf, entry.size)) {
                return false;
            }
        }
        return true;
    }

    // Process in batches of SCATTER_MAX_ENTRIES
    size_t total_entries = entries.size();
    size_t offset = 0;

    while (offset < total_entries) {
        size_t batch_size = std::min<size_t>(SCATTER_MAX_ENTRIES, total_entries - offset);

        // Zero the shared page
        memset(m_shared_page_va, 0, 4096);

        // Write header at offset 0
        HvScatterHeader* header = reinterpret_cast<HvScatterHeader*>(m_shared_page_va);
        header->request_count = static_cast<uint32_t>(batch_size);
        header->status = 0;
        header->target_cr3 = m_cr3;

        // Write entries starting at offset 16
        HvScatterEntry* hv_entries = reinterpret_cast<HvScatterEntry*>(
            static_cast<uint8_t*>(m_shared_page_va) + 16);

        for (size_t i = 0; i < batch_size; i++) {
            const ScatterEntry& user_entry = entries[offset + i];
            HvScatterEntry& hv_entry = hv_entries[i];

            hv_entry.src_va = user_entry.va;
            hv_entry.size = user_entry.size;
            hv_entry.status = 0;
            hv_entry.reserved = 0;
            hv_entry.result = 0;
            hv_entry.reserved2 = 0;
        }

        // Execute scatter read
        uint64_t status = hv::call(hv::CMD_SCATTER_READ, 0, 0, 0, 0);
        if (status != 0) {
            spdlog::error("Scatter read command failed: 0x{:X}", status);
            return false;
        }

        // Read back header
        if (header->status != SCATTER_STATUS_COMPLETE) {
            spdlog::error("Scatter read not complete: header status=0x{:X}", header->status);
            return false;
        }

        // Read back entries and copy results
        for (size_t i = 0; i < batch_size; i++) {
            const HvScatterEntry& hv_entry = hv_entries[i];
            if (hv_entry.status != HYPE_MEM_OK) {
                spdlog::error("Scatter entry {} failed: status=0x{:X}, va=0x{:X}",
                    i, hv_entry.status, hv_entry.src_va);
                return false;
            }

            // Copy result bytes into user buffer
            ScatterEntry& user_entry = entries[offset + i];
            memcpy(user_entry.output_buf, &hv_entry.result, user_entry.size);
        }

        spdlog::trace("Scatter read batch complete: {} entries", batch_size);
        offset += batch_size;
    }

    return true;
}

uint64_t HvMemory::translate(uint64_t va) {
    if (!attached()) {
        return 0;
    }

    // Check cache (page-aligned)
    uint64_t page_va = va & ~0xFFFULL;
    auto it = m_va_cache.find(page_va);
    if (it != m_va_cache.end()) {
        return it->second + (va & 0xFFF);
    }

    // Cache miss - call hypervisor
    uint64_t pa = hv::call(hv::CMD_TRANSLATE_VA, m_cr3, va, 0, 0);
    if (pa == 0 || pa == ~0ULL) {
        return 0;
    }

    // Cache the page translation
    if (m_va_cache.size() >= VA_CACHE_MAX) {
        m_va_cache.clear();  // Simple eviction
    }
    m_va_cache[page_va] = pa & ~0xFFFULL;

    return pa;
}

bool HvMemory::scatter_write(const std::vector<ScatterWriteEntry>& entries) {
    if (!attached() || entries.empty()) {
        return false;
    }

    // Initialize shared page if needed
    if (!m_shared_page_initialized && !init_shared_page()) {
        spdlog::warn("Shared page unavailable, falling back to individual writes");
        for (const auto& entry : entries) {
            if (!write_raw(entry.va, &entry.value, entry.size)) {
                return false;
            }
        }
        return true;
    }

    // Process in batches of SCATTER_MAX_ENTRIES
    size_t total_entries = entries.size();
    size_t offset = 0;

    while (offset < total_entries) {
        size_t batch_size = std::min<size_t>(SCATTER_MAX_ENTRIES, total_entries - offset);

        // Zero the shared page
        memset(m_shared_page_va, 0, 4096);

        // Write header at offset 0
        HvScatterHeader* header = reinterpret_cast<HvScatterHeader*>(m_shared_page_va);
        header->request_count = static_cast<uint32_t>(batch_size);
        header->status = 0;
        header->target_cr3 = m_cr3;

        // Write entries starting at offset 16
        HvScatterEntry* hv_entries = reinterpret_cast<HvScatterEntry*>(
            static_cast<uint8_t*>(m_shared_page_va) + 16);

        for (size_t i = 0; i < batch_size; i++) {
            const ScatterWriteEntry& user_entry = entries[offset + i];
            HvScatterEntry& hv_entry = hv_entries[i];

            hv_entry.src_va = user_entry.va;
            hv_entry.size = user_entry.size;
            hv_entry.status = 0;
            hv_entry.reserved = 0;
            hv_entry.result = user_entry.value;  // INPUT value for write
            hv_entry.reserved2 = 0;
        }

        // Execute scatter write
        uint64_t status = hv::call(hv::CMD_SCATTER_WRITE, 0, 0, 0, 0);
        if (status != 0) {
            spdlog::error("Scatter write command failed: 0x{:X}", status);
            return false;
        }

        // Read back header
        if (header->status != SCATTER_STATUS_COMPLETE) {
            spdlog::error("Scatter write not complete: header status=0x{:X}", header->status);
            return false;
        }

        // Check per-entry status
        for (size_t i = 0; i < batch_size; i++) {
            const HvScatterEntry& hv_entry = hv_entries[i];
            if (hv_entry.status != HYPE_MEM_OK) {
                spdlog::error("Scatter write entry {} failed: status=0x{:X}, va=0x{:X}",
                    i, hv_entry.status, hv_entry.src_va);
                return false;
            }
        }

        spdlog::trace("Scatter write batch complete: {} entries", batch_size);
        offset += batch_size;
    }

    return true;
}

uint64_t HvMemory::djb2_hash(const char* str) {
    uint64_t hash = 5381;

    while (*str) {
        // Case-insensitive, treat as wide char (zero-extend byte)
        wchar_t wc = static_cast<wchar_t>(std::tolower(static_cast<unsigned char>(*str)));
        hash = ((hash << 5) + hash) ^ wc;
        str++;
    }

    return hash;
}
