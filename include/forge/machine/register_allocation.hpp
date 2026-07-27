#pragma once

#include <cstdint>
#include <vector>

#include "forge/diagnostics/diagnostic.hpp"
#include "forge/machine/module.hpp"

namespace forge::machine {

enum class PhysicalRegister : std::uint8_t {
    r10d,
    r11d,
    r12d,
    r13d,
};

[[nodiscard]] constexpr bool is_call_clobbered(PhysicalRegister reg) noexcept {
    return reg == PhysicalRegister::r10d || reg == PhysicalRegister::r11d;
}

[[nodiscard]] constexpr bool is_callee_saved(PhysicalRegister reg) noexcept {
    return reg == PhysicalRegister::r12d || reg == PhysicalRegister::r13d;
}

enum class FloatingPhysicalRegister : std::uint8_t {
    xmm2,
    xmm3,
    xmm4,
    xmm5,
};

enum class LocationKind : std::uint8_t {
    physical_register,
    floating_register,
    stack_slot,
    rematerialized_integer,
    rematerialized_floating,
};

struct AllocationLocation {
    LocationKind kind{LocationKind::stack_slot};
    PhysicalRegister physical{PhysicalRegister::r10d};
    FloatingPhysicalRegister floating{FloatingPhysicalRegister::xmm2};
    std::int32_t stack_offset{};
    std::int64_t rematerialized_immediate{};
};

struct LiveInterval {
    VirtualRegister virtual_register{};
    std::uint32_t start{};
    std::uint32_t end{};
    std::uint32_t use_count{};
    std::uint32_t spill_weight{};
    std::uint32_t loop_depth{};
    std::uint32_t call_crossing_count{};
};

struct RegisterAllocation {
    std::vector<AllocationLocation> locations;
    std::vector<LiveInterval> intervals;
    std::uint32_t frame_size{};
    std::uint32_t physical_count{};
    std::uint32_t spill_count{};
    std::uint32_t spill_slot_count{};
    std::uint32_t reused_spill_slot_count{};
    std::uint32_t frame_size_before_slot_reuse{};
    std::uint32_t frame_bytes_saved{};
    std::uint32_t coalesced_copy_count{};
    std::uint32_t two_address_reuse_count{};
    std::uint32_t unary_reuse_count{};
    std::uint32_t rematerialized_value_count{};
    std::uint32_t rematerialized_use_count{};
    std::uint32_t peak_integer_pressure{};
    std::uint32_t peak_floating_pressure{};
    std::uint32_t call_crossing_interval_count{};
    std::uint32_t caller_saved_allocation_count{};
    std::uint32_t callee_saved_allocation_count{};
    std::uint32_t weighted_spill_decision_count{};
    std::uint32_t copy_hint_count{};
    Diagnostics diagnostics;

    [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
    [[nodiscard]] const AllocationLocation& location(VirtualRegister reg) const { return locations.at(reg); }
};

struct StackAllocation {
    std::vector<std::int32_t> offsets;
    std::uint32_t frame_size{};
    Diagnostics diagnostics;

    [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
    [[nodiscard]] std::int32_t offset(VirtualRegister reg) const { return offsets.at(reg); }
};

[[nodiscard]] std::vector<LiveInterval> compute_live_intervals(const Function& function);
[[nodiscard]] RegisterAllocation allocate_linear_scan(const Function& function);
[[nodiscard]] StackAllocation allocate_stack_slots(const Function& function);

} // namespace forge::machine
