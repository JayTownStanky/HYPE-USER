#ifndef MEM_H
#define MEM_H

#include "hv_comms.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>
#include <string>

// ============================================================================
// Hypervisor Wire Format (matches HypeMemory.h exactly)
// ============================================================================

#pragma pack(push, 1)

struct HvScatterHeader {
    uint32_t request_count;
    uint32_t status;
    uint64_t target_cr3;
};
static_assert(sizeof(HvScatterHeader) == 16, "HvScatterHeader must be 16 bytes");

struct HvScatterEntry {
    uint64_t src_va;
    uint16_t size;
    uint16_t status;
    uint32_t reserved;
    uint64_t result;
    uint64_t reserved2;
};
static_assert(sizeof(HvScatterEntry) == 32, "HvScatterEntry must be 32 bytes");

#pragma pack(pop)

// Constants
constexpr uint32_t SCATTER_MAX_ENTRIES = 127;
constexpr uint32_t SCATTER_STATUS_COMPLETE = 0x01;
constexpr uint32_t HYPE_MEM_OK = 0x00;

// ============================================================================
// User-Facing API Structs
// ============================================================================

// Scatter read entry
struct ScatterEntry {
    uint64_t va;
    uint16_t size;
    void* output_buf;
};

// Scatter write entry
struct ScatterWriteEntry {
    uint64_t va;
    uint16_t size;
    uint64_t value;
};

class HvMemory {
public:
    HvMemory() = default;
    ~HvMemory() = default;

    // Attach to process by PID using EPROCESS list walk
    // eprocess_va: virtual address of any EPROCESS (usually System)
    bool attach(uint32_t pid, uint64_t eprocess_va);

    // Get module base by name (DJB2 hash, case-insensitive)
    uint64_t get_module_base(const char* name);

    // Read value at virtual address
    template<typename T>
    std::optional<T> read(uint64_t va) {
        uint64_t pa = translate(va);
        if (pa == 0 || pa == ~0ULL) {
            return std::nullopt;
        }
        T value{};
        uint64_t status = hv::call(hv::CMD_READ_PHYS, pa, sizeof(T),
                                   reinterpret_cast<uint64_t>(&value), 0);
        if (status != 0) {
            return std::nullopt;
        }
        return value;
    }

    // Write value at virtual address
    template<typename T>
    bool write(uint64_t va, const T& value) {
        uint64_t pa = translate(va);
        if (pa == 0 || pa == ~0ULL) {
            return false;
        }
        uint64_t status = hv::call(hv::CMD_WRITE_PHYS, pa, sizeof(T),
                                   reinterpret_cast<uint64_t>(&value), 0);
        return (status == 0);
    }

    // Read raw bytes
    bool read_raw(uint64_t va, void* buf, size_t size);

    // Write raw bytes
    bool write_raw(uint64_t va, const void* buf, size_t size);

    // Scatter read - batch multiple reads
    bool scatter_read(std::vector<ScatterEntry>& entries);

    // Scatter write - batch multiple writes
    bool scatter_write(const std::vector<ScatterWriteEntry>& entries);

    // Translate VA to PA using cached CR3
    uint64_t translate(uint64_t va);

    // Get cached CR3
    uint64_t cr3() const { return m_cr3; }

    // Check if attached
    bool attached() const { return m_cr3 != 0; }

private:
    // Initialize shared page for scatter operations
    bool init_shared_page();

    uint64_t m_cr3 = 0;
    uint64_t m_eprocess = 0;
    uint32_t m_pid = 0;

    // Shared page state
    void* m_shared_page_va = nullptr;
    uint64_t m_shared_page_gpa = 0;
    bool m_shared_page_initialized = false;

    // EPROCESS offsets (Win10 22H2 / Win11)
    static constexpr uint64_t OFF_ACTIVE_PROCESS_LINKS = 0x448;
    static constexpr uint64_t OFF_UNIQUE_PROCESS_ID    = 0x440;
    static constexpr uint64_t OFF_DIRECTORY_TABLE_BASE = 0x028;

    // VA translation cache
    std::unordered_map<uint64_t, uint64_t> m_va_cache;
    static constexpr size_t VA_CACHE_MAX = 4096;

    // DJB2 hash (case-insensitive, wide-char style)
    static uint64_t djb2_hash(const char* str);
};

#endif // MEM_H
