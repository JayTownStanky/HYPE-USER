#include "hv_comms.h"
#include <intrin.h>

namespace hv {

// Active communication channel
static Channel s_channel = Channel::NONE;

// CPUID magic leaf for backdoor
constexpr uint32_t CPUID_MAGIC_LEAF = 0x48595045;

// Try VMMCALL ping with SEH protection
static bool try_vmmcall_ping() {
    __try {
        uint64_t result = hv_vmmcall(CMD_PING, AUTH_KEY, 0, 0, 0, 0);
        return (result == PING_RESPONSE);
    }
    __except (1) {
        // #UD or other exception - no hypervisor via VMMCALL
        return false;
    }
}

// Try CPUID backdoor ping
static bool try_cpuid_ping() {
    int regs[4] = {0};
    __cpuidex(regs, CPUID_MAGIC_LEAF, 0);
    return (static_cast<uint32_t>(regs[0]) == PING_RESPONSE);
}

bool init() {
    s_channel = Channel::NONE;

    // Try VMMCALL first (preferred - faster, more params)
    if (try_vmmcall_ping()) {
        s_channel = Channel::VMMCALL;
        return true;
    }

    // Fall back to CPUID backdoor
    if (try_cpuid_ping()) {
        s_channel = Channel::CPUID;
        return true;
    }

    return false;
}

Channel active_channel() {
    return s_channel;
}

uint64_t call(uint64_t cmd, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4) {
    if (s_channel == Channel::VMMCALL) {
        return hv_vmmcall(cmd, AUTH_KEY, p1, p2, p3, p4);
    }

    if (s_channel == Channel::CPUID) {
        // CPUID path - limited to 32-bit params via staging
        // Only supports subset of commands
        int regs[4] = {0};
        __cpuidex(regs, CPUID_MAGIC_LEAF, static_cast<int>(cmd));
        return static_cast<uint64_t>(regs[0]);
    }

    // No channel available
    return ~0ULL;
}

} // namespace hv
