#ifndef HV_COMMS_H
#define HV_COMMS_H

#include <cstdint>
#include <optional>

// NASM stub - VMMCALL with register shuffle
extern "C" uint64_t hv_vmmcall(uint64_t cmd, uint64_t auth,
                                uint64_t p1, uint64_t p2,
                                uint64_t p3, uint64_t p4);

namespace hv {

// Communication channel type
enum class Channel {
    NONE,
    VMMCALL,
    CPUID
};

// Command IDs (must match hypervisor protocol)
constexpr uint64_t CMD_PING           = 0x00;
constexpr uint64_t CMD_VERSION        = 0x01;
constexpr uint64_t CMD_READ_PHYS      = 0x10;
constexpr uint64_t CMD_WRITE_PHYS     = 0x11;
constexpr uint64_t CMD_TRANSLATE_VA   = 0x20;
constexpr uint64_t CMD_GET_CR3        = 0x21;
constexpr uint64_t CMD_GET_MODULE_BASE = 0x22;
constexpr uint64_t CMD_SET_EPROCESS   = 0x23;
constexpr uint64_t CMD_HIDE_PAGE      = 0x30;
constexpr uint64_t CMD_UNHIDE_PAGE    = 0x31;
constexpr uint64_t CMD_SHARED_INIT    = 0x40;
constexpr uint64_t CMD_SCATTER_READ   = 0x41;
constexpr uint64_t CMD_SCATTER_WRITE  = 0x42;

// Authentication key
constexpr uint64_t AUTH_KEY = 0x48595045DEADBEEF;

// Expected ping response ('HYPE')
constexpr uint64_t PING_RESPONSE = 0x48595045;

// Initialize communication - detect active channel
bool init();

// Get currently active channel
Channel active_channel();

// Execute hypervisor command (uses detected channel)
uint64_t call(uint64_t cmd, uint64_t p1 = 0, uint64_t p2 = 0,
              uint64_t p3 = 0, uint64_t p4 = 0);

} // namespace hv

#endif // HV_COMMS_H
