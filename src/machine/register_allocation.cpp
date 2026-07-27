#include "forge/machine/register_allocation.hpp"
#include "forge/machine/liveness.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <unordered_map>

namespace forge::machine {
namespace {
constexpr std::uint32_t undefined_position = std::numeric_limits<std::uint32_t>::max();

void touch(std::vector<LiveInterval>& intervals, VirtualRegister reg, std::uint32_t position) {
    if (reg >= intervals.size()) return;
    auto& interval = intervals[reg];
    interval.start = std::min(interval.start, position);
    interval.end = std::max(interval.end, position);
}

std::uint32_t align_frame(std::uint32_t bytes) {
    return (bytes + 15U) & ~15U;
}

bool is_commutative_two_address(Opcode opcode) {
    switch (opcode) {
    case Opcode::add_i32: case Opcode::mul_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32:
    case Opcode::add_i64: case Opcode::mul_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64:
    case Opcode::add_f32: case Opcode::mul_f32:
    case Opcode::add_f64: case Opcode::mul_f64:
        return true;
    default:
        return false;
    }
}

bool supports_unary_reuse(Opcode opcode) {
    switch (opcode) {
    case Opcode::neg_i32: case Opcode::not_i32:
    case Opcode::neg_i64: case Opcode::not_i64:
    case Opcode::neg_f32: case Opcode::neg_f64:
        return true;
    default:
        return false;
    }
}

bool supports_two_address_reuse(Opcode opcode) {
    switch (opcode) {
    case Opcode::add_i32: case Opcode::sub_i32: case Opcode::mul_i32:
    case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32:
    case Opcode::add_i64: case Opcode::sub_i64: case Opcode::mul_i64:
    case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64:
    case Opcode::add_f32: case Opcode::sub_f32: case Opcode::mul_f32: case Opcode::div_f32:
    case Opcode::add_f64: case Opcode::sub_f64: case Opcode::mul_f64: case Opcode::div_f64:
        return true;
    default:
        return false;
    }
}

bool produces_result(Opcode opcode) {
    switch (opcode) {
    case Opcode::store_stack_i8: case Opcode::store_stack_i16: case Opcode::store_stack_i32:
    case Opcode::store_stack_i64: case Opcode::store_stack_f32: case Opcode::store_stack_f64:
    case Opcode::store_ptr_i8: case Opcode::store_ptr_i16: case Opcode::store_ptr_i32:
    case Opcode::store_ptr_i64: case Opcode::store_ptr_f32: case Opcode::store_ptr_f64:
    case Opcode::call_void: case Opcode::call_indirect_void:
    case Opcode::jump: case Opcode::branch_i1:
    case Opcode::return_i32: case Opcode::return_i64: case Opcode::return_f32:
    case Opcode::return_f64: case Opcode::return_void:
        return false;
    default:
        return true;
    }
}
} // namespace

std::vector<LiveInterval> compute_live_intervals(const Function& function) {
    std::vector<LiveInterval> intervals(function.register_count);
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg)
        intervals[reg] = {reg, undefined_position, 0, 0, 0, 0, 0};

    const auto block_count = function.blocks.size();
    std::unordered_map<std::string, std::size_t> block_indices;
    for (std::size_t index = 0; index < block_count; ++index)
        block_indices.emplace(function.blocks[index].name, index);

    const auto liveness = analyze_liveness(function);
    const auto& live_in = liveness.live_in;
    const auto& live_out = liveness.live_out;
    const auto& successors = liveness.successors;
    std::vector<std::vector<bool>> defs(block_count, std::vector<bool>(function.register_count));
    std::vector<std::uint32_t> block_starts(block_count);
    std::vector<std::uint32_t> block_ends(block_count);

    std::uint32_t position = 0;
    for (std::size_t block_index = 0; block_index < block_count; ++block_index) {
        const auto& block = function.blocks[block_index];
        block_starts[block_index] = position;
        for (const auto parameter : block.parameters) {
            if (parameter < function.register_count) defs[block_index][parameter] = true;
            touch(intervals, parameter, position);
        }
        ++position;
        for (const auto& instruction : block.instructions) {
            const bool loop_weighted = std::any_of(instruction.successors.begin(), instruction.successors.end(),
                [&](const Successor& successor) {
                    const auto target = block_indices.find(successor.block);
                    return target != block_indices.end() && target->second <= block_index;
                });
            const auto record_use = [&](VirtualRegister reg, std::uint32_t weight = 1U) {
                if (reg >= function.register_count) return;
                touch(intervals, reg, position);
                ++intervals[reg].use_count;
                intervals[reg].spill_weight += weight * (loop_weighted ? 8U : 1U);
            };
            for (const auto input : instruction.inputs) record_use(input, 2U);
            for (const auto& successor : instruction.successors) {
                for (const auto argument : successor.arguments) record_use(argument, 4U);
            }
            if (produces_result(instruction.opcode) && instruction.result < function.register_count) {
                defs[block_index][instruction.result] = true;
                touch(intervals, instruction.result, position);
                ++intervals[instruction.result].spill_weight;
            }
            ++position;
        }
        block_ends[block_index] = position;
    }

    // Approximate natural-loop depth from backward CFG edges. Every block in the
    // target..source range receives one additional nesting level. This is
    // deterministic, inexpensive, and substantially more representative than
    // weighting only the terminator that carries the backedge.
    std::vector<std::uint32_t> block_loop_depth(block_count, 0U);
    for (std::size_t source = 0; source < block_count; ++source) {
        for (const auto target : successors[source]) {
            if (target > source) continue;
            for (std::size_t member = target; member <= source; ++member)
                ++block_loop_depth[member];
        }
    }

    // Recompute weighted use costs with the block loop depth now known.
    for (auto& interval : intervals) interval.spill_weight = interval.use_count == 0 ? 0U : 1U;
    position = 0;
    for (std::size_t block_index = 0; block_index < block_count; ++block_index) {
        ++position;
        const auto depth = block_loop_depth[block_index];
        const auto loop_multiplier = depth == 0U ? 1U : (depth == 1U ? 8U : 32U);
        for (const auto& instruction : function.blocks[block_index].instructions) {
            for (const auto input : instruction.inputs) {
                if (input < intervals.size()) {
                    intervals[input].spill_weight += 2U * loop_multiplier;
                    intervals[input].loop_depth = std::max(intervals[input].loop_depth, depth);
                }
            }
            for (const auto& successor : instruction.successors) {
                for (const auto argument : successor.arguments) {
                    if (argument < intervals.size()) {
                        intervals[argument].spill_weight += 4U * loop_multiplier;
                        intervals[argument].loop_depth = std::max(intervals[argument].loop_depth, depth);
                    }
                }
            }
            ++position;
        }
    }


    for (std::size_t block_index = 0; block_index < block_count; ++block_index) {
        for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
            if (live_in[block_index][reg]) touch(intervals, reg, block_starts[block_index]);
            if (live_out[block_index][reg]) touch(intervals, reg, block_ends[block_index]);
        }
    }

    for (auto& interval : intervals) {
        if (interval.start == undefined_position) interval.start = interval.end = 0;
    }
    return intervals;
}

namespace {
std::uint64_t spill_priority(const LiveInterval& interval) {
    const auto length = static_cast<std::uint64_t>(interval.end - interval.start + 1U);
    return (static_cast<std::uint64_t>(interval.spill_weight) + 1U) * 1024U / length;
}

template <typename Active>
bool should_spill_active(const Active& active, const LiveInterval& incoming) {
    const auto active_priority = spill_priority(active.interval);
    const auto incoming_priority = spill_priority(incoming);
    if (active_priority != incoming_priority) return active_priority < incoming_priority;
    return active.interval.end > incoming.end;
}
} // namespace

RegisterAllocation allocate_linear_scan(const Function& function) {
    RegisterAllocation allocation;
    if (function.register_count > 16384U) {
        allocation.diagnostics.push_back({DiagnosticSeverity::error,
            "linear-scan virtual-register limit exceeded in @" + function.name, {}});
        return allocation;
    }

    allocation.intervals = compute_live_intervals(function);
    allocation.locations.resize(function.register_count);

    std::vector<std::uint32_t> call_positions;
    std::vector<bool> forced_floating_spill(function.register_count, false);
    std::vector<VirtualRegister> copy_sources(function.register_count, function.register_count);
    std::vector<VirtualRegister> two_address_sources(function.register_count, function.register_count);
    std::vector<VirtualRegister> unary_sources(function.register_count, function.register_count);
    std::uint32_t position = 0;
    for (const auto& block : function.blocks) {
        ++position;
        for (const auto& instruction : block.instructions) {
            if ((instruction.opcode == Opcode::copy || instruction.opcode == Opcode::copy_f32 ||
                 instruction.opcode == Opcode::copy_f64) && instruction.inputs.size() == 1 &&
                instruction.result < function.register_count)
                copy_sources[instruction.result] = instruction.inputs.front(), ++allocation.copy_hint_count;
            if (supports_two_address_reuse(instruction.opcode) && instruction.inputs.size() == 2 &&
                instruction.result < function.register_count) {
                auto source = instruction.inputs.front();
                if (is_commutative_two_address(instruction.opcode)) {
                    const auto right = instruction.inputs[1];
                    const auto left_dies = source < allocation.intervals.size() &&
                                           allocation.intervals[source].end == position;
                    const auto right_dies = right < allocation.intervals.size() &&
                                            allocation.intervals[right].end == position;
                    if (!left_dies && right_dies) source = right;
                }
                two_address_sources[instruction.result] = source;
            }
            if (supports_unary_reuse(instruction.opcode) && instruction.inputs.size() == 1 &&
                instruction.result < function.register_count)
                unary_sources[instruction.result] = instruction.inputs.front();
            switch (instruction.opcode) {
            case Opcode::call_i32: case Opcode::call_i64: case Opcode::call_f32: case Opcode::call_f64: case Opcode::call_void:
            case Opcode::call_indirect_i32: case Opcode::call_indirect_i64: case Opcode::call_indirect_f32:
            case Opcode::call_indirect_f64: case Opcode::call_indirect_void:
                call_positions.push_back(position);
                for (const auto reg : instruction.inputs)
                    if (reg < function.register_count && reg < function.register_classes.size() &&
                        function.register_classes[reg] == RegisterClass::floating)
                        forced_floating_spill[reg] = true;
                break;
            default: break;
            }
            if ((instruction.opcode == Opcode::load_argument_f32 || instruction.opcode == Opcode::load_argument_f64) &&
                instruction.result < function.register_count)
                forced_floating_spill[instruction.result] = true;
            ++position;
        }
    }
    const auto crosses_call = [&](const LiveInterval& interval) {
        return std::any_of(call_positions.begin(), call_positions.end(), [&](std::uint32_t call) {
            return interval.start < call && call < interval.end;
        });
    };
    for (auto& interval : allocation.intervals) {
        interval.call_crossing_count = static_cast<std::uint32_t>(std::count_if(
            call_positions.begin(), call_positions.end(), [&](std::uint32_t call) {
                return interval.start < call && call < interval.end;
            }));
        if (interval.call_crossing_count != 0U) ++allocation.call_crossing_interval_count;
    }

    // Measure true interval overlap pressure independently per register class.
    for (std::uint32_t scan = 0; scan <= position; ++scan) {
        std::uint32_t integer_pressure = 0;
        std::uint32_t floating_pressure = 0;
        for (const auto& interval : allocation.intervals) {
            if (interval.start > scan || interval.end < scan || interval.use_count == 0U) continue;
            const bool floating = interval.virtual_register < function.register_classes.size() &&
                                  function.register_classes[interval.virtual_register] == RegisterClass::floating;
            if (floating) ++floating_pressure; else ++integer_pressure;
        }
        allocation.peak_integer_pressure = std::max(allocation.peak_integer_pressure, integer_pressure);
        allocation.peak_floating_pressure = std::max(allocation.peak_floating_pressure, floating_pressure);
    }

    std::vector<LiveInterval> ordered = allocation.intervals;
    std::stable_sort(ordered.begin(), ordered.end(), [](const LiveInterval& left, const LiveInterval& right) {
        if (left.start != right.start) return left.start < right.start;
        return left.virtual_register < right.virtual_register;
    });

    struct IntegerActive { LiveInterval interval; PhysicalRegister physical; };
    struct FloatingActive { LiveInterval interval; FloatingPhysicalRegister physical; };
    std::vector<IntegerActive> integer_active;
    std::vector<FloatingActive> floating_active;
    constexpr std::array integer_physicals{PhysicalRegister::r10d, PhysicalRegister::r11d, PhysicalRegister::r12d, PhysicalRegister::r13d};
    constexpr std::array floating_physicals{FloatingPhysicalRegister::xmm2, FloatingPhysicalRegister::xmm3, FloatingPhysicalRegister::xmm4, FloatingPhysicalRegister::xmm5};
    std::vector<PhysicalRegister> free_integer(integer_physicals.begin(), integer_physicals.end());
    std::vector<FloatingPhysicalRegister> free_floating(floating_physicals.begin(), floating_physicals.end());
    const auto take_integer_register = [&](bool across_call) -> std::optional<PhysicalRegister> {
        const auto preferred = std::find_if(free_integer.begin(), free_integer.end(), [&](PhysicalRegister reg) {
            return across_call ? is_callee_saved(reg) : is_call_clobbered(reg);
        });
        if (preferred != free_integer.end()) {
            const auto physical = *preferred;
            free_integer.erase(preferred);
            return physical;
        }
        if (across_call) return std::nullopt;
        if (free_integer.empty()) return std::nullopt;
        const auto physical = free_integer.back();
        free_integer.pop_back();
        return physical;
    };
    const auto record_integer_allocation = [&](PhysicalRegister physical) {
        if (is_callee_saved(physical)) ++allocation.callee_saved_allocation_count;
        else ++allocation.caller_saved_allocation_count;
    };
    auto spill = [&](VirtualRegister reg) {
        allocation.locations[reg] = {LocationKind::stack_slot, PhysicalRegister::r10d,
                                     FloatingPhysicalRegister::xmm2, 0};
        ++allocation.spill_count;
    };

    for (const auto& interval : ordered) {
        const bool floating = interval.virtual_register < function.register_classes.size() &&
                              function.register_classes[interval.virtual_register] == RegisterClass::floating;
        if (floating) {
            for (auto iterator = floating_active.begin(); iterator != floating_active.end();) {
                if (iterator->interval.end < interval.start) {
                    free_floating.push_back(iterator->physical);
                    iterator = floating_active.erase(iterator);
                } else ++iterator;
            }
            bool coalesced = false;
            const auto copy_source = copy_sources[interval.virtual_register];
            const auto arithmetic_source = two_address_sources[interval.virtual_register];
            const auto unary_source = unary_sources[interval.virtual_register];
            const auto source = copy_source < function.register_count ? copy_source :
                                arithmetic_source < function.register_count ? arithmetic_source : unary_source;
            if (!crosses_call(interval) && !forced_floating_spill[interval.virtual_register] &&
                source < function.register_count) {
                const auto source_active = std::find_if(floating_active.begin(), floating_active.end(),
                    [&](const FloatingActive& active) {
                        return active.interval.virtual_register == source && active.interval.end == interval.start;
                    });
                if (source_active != floating_active.end()) {
                    const auto physical = source_active->physical;
                    floating_active.erase(source_active);
                    allocation.locations[interval.virtual_register] = {
                        LocationKind::floating_register, PhysicalRegister::r10d, physical, 0};
                    floating_active.push_back({interval, physical});
                    ++allocation.physical_count;
                    if (copy_source < function.register_count) ++allocation.coalesced_copy_count;
                    else if (arithmetic_source < function.register_count) ++allocation.two_address_reuse_count;
                    else ++allocation.unary_reuse_count;
                    coalesced = true;
                }
            }
            if (coalesced) {
                std::sort(floating_active.begin(), floating_active.end(),
                    [](const FloatingActive& left, const FloatingActive& right) { return left.interval.end < right.interval.end; });
                continue;
            }
            if (crosses_call(interval) || forced_floating_spill[interval.virtual_register]) {
                spill(interval.virtual_register);
            } else if (!free_floating.empty()) {
                const auto physical = free_floating.back();
                free_floating.pop_back();
                allocation.locations[interval.virtual_register] = {
                    LocationKind::floating_register, PhysicalRegister::r10d, physical, 0};
                floating_active.push_back({interval, physical});
                ++allocation.physical_count;
            } else {
                auto candidate = std::min_element(floating_active.begin(), floating_active.end(),
                    [](const FloatingActive& left, const FloatingActive& right) {
                        const auto left_priority = spill_priority(left.interval);
                        const auto right_priority = spill_priority(right.interval);
                        if (left_priority != right_priority) return left_priority < right_priority;
                        return left.interval.end > right.interval.end;
                    });
                if (candidate != floating_active.end() && should_spill_active(*candidate, interval)) {
                    const auto physical = candidate->physical;
                    spill(candidate->interval.virtual_register);
                    ++allocation.weighted_spill_decision_count;
                    *candidate = {interval, physical};
                    allocation.locations[interval.virtual_register] = {
                        LocationKind::floating_register, PhysicalRegister::r10d, physical, 0};
                    ++allocation.physical_count;
                } else spill(interval.virtual_register);
            }
            std::sort(floating_active.begin(), floating_active.end(),
                [](const FloatingActive& left, const FloatingActive& right) { return left.interval.end < right.interval.end; });
            continue;
        }

        for (auto iterator = integer_active.begin(); iterator != integer_active.end();) {
            if (iterator->interval.end < interval.start) {
                free_integer.push_back(iterator->physical);
                iterator = integer_active.erase(iterator);
            } else ++iterator;
        }
        bool coalesced = false;
        const auto copy_source = copy_sources[interval.virtual_register];
        const auto arithmetic_source = two_address_sources[interval.virtual_register];
        const auto unary_source = unary_sources[interval.virtual_register];
        const auto source = copy_source < function.register_count ? copy_source :
                            arithmetic_source < function.register_count ? arithmetic_source : unary_source;
        if (source < function.register_count) {
            const auto source_active = std::find_if(integer_active.begin(), integer_active.end(),
                [&](const IntegerActive& active) {
                    return active.interval.virtual_register == source && active.interval.end == interval.start;
                });
            if (source_active != integer_active.end() &&
                (!crosses_call(interval) || is_callee_saved(source_active->physical))) {
                const auto physical = source_active->physical;
                integer_active.erase(source_active);
                allocation.locations[interval.virtual_register] = {
                    LocationKind::physical_register, physical, FloatingPhysicalRegister::xmm2, 0};
                integer_active.push_back({interval, physical});
                ++allocation.physical_count;
                record_integer_allocation(physical);
                if (copy_source < function.register_count) ++allocation.coalesced_copy_count;
                else if (arithmetic_source < function.register_count) ++allocation.two_address_reuse_count;
                else ++allocation.unary_reuse_count;
                coalesced = true;
            }
        }
        if (coalesced) {
            std::sort(integer_active.begin(), integer_active.end(),
                [](const IntegerActive& left, const IntegerActive& right) { return left.interval.end < right.interval.end; });
            continue;
        }
        const bool interval_crosses_call = crosses_call(interval);
        if (const auto selected = take_integer_register(interval_crosses_call)) {
            const auto physical = *selected;
            allocation.locations[interval.virtual_register] = {
                LocationKind::physical_register, physical, FloatingPhysicalRegister::xmm2, 0};
            integer_active.push_back({interval, physical});
            ++allocation.physical_count;
            record_integer_allocation(physical);
        } else {
            auto candidate = std::min_element(integer_active.begin(), integer_active.end(),
                [&](const IntegerActive& left, const IntegerActive& right) {
                    const bool left_eligible = !interval_crosses_call || is_callee_saved(left.physical);
                    const bool right_eligible = !interval_crosses_call || is_callee_saved(right.physical);
                    if (left_eligible != right_eligible) return left_eligible;
                    const auto left_priority = spill_priority(left.interval);
                    const auto right_priority = spill_priority(right.interval);
                    if (left_priority != right_priority) return left_priority < right_priority;
                    return left.interval.end > right.interval.end;
                });
            if (candidate != integer_active.end() &&
                (!interval_crosses_call || is_callee_saved(candidate->physical)) &&
                should_spill_active(*candidate, interval)) {
                const auto physical = candidate->physical;
                spill(candidate->interval.virtual_register);
                ++allocation.weighted_spill_decision_count;
                *candidate = {interval, physical};
                allocation.locations[interval.virtual_register] = {
                    LocationKind::physical_register, physical, FloatingPhysicalRegister::xmm2, 0};
                ++allocation.physical_count;
                record_integer_allocation(physical);
            } else spill(interval.virtual_register);
        }
        std::sort(integer_active.begin(), integer_active.end(),
            [](const IntegerActive& left, const IntegerActive& right) { return left.interval.end < right.interval.end; });
    }

    // Replace stack-backed constants with rematerialized locations before
    // assigning physical spill slots. Recreating these values at each use is
    // cheaper than reserving frame storage and emitting store/load traffic.
    std::vector<const Instruction*> definitions(function.register_count, nullptr);
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (produces_result(instruction.opcode) && instruction.result < function.register_count)
                definitions[instruction.result] = &instruction;
        }
    }
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
        auto& location = allocation.locations[reg];
        if (location.kind != LocationKind::stack_slot || definitions[reg] == nullptr ||
            allocation.intervals[reg].use_count != 1U) continue;
        const auto opcode = definitions[reg]->opcode;
        const bool integer = opcode == Opcode::load_immediate || opcode == Opcode::load_immediate_i64;
        const bool floating = opcode == Opcode::load_immediate_f32 || opcode == Opcode::load_immediate_f64;
        if (!integer && !floating) continue;
        location.kind = integer ? LocationKind::rematerialized_integer : LocationKind::rematerialized_floating;
        location.rematerialized_immediate = definitions[reg]->immediate;
        ++allocation.rematerialized_value_count;
        allocation.rematerialized_use_count += allocation.intervals[reg].use_count;
    }

    struct ReusableSlot { std::uint32_t end{}; std::uint32_t index{}; };
    std::vector<LiveInterval> spilled_intervals;
    spilled_intervals.reserve(allocation.spill_count);
    for (const auto& interval : allocation.intervals)
        if (allocation.locations[interval.virtual_register].kind == LocationKind::stack_slot)
            spilled_intervals.push_back(interval);
    std::stable_sort(spilled_intervals.begin(), spilled_intervals.end(),
        [](const LiveInterval& left, const LiveInterval& right) {
            if (left.start != right.start) return left.start < right.start;
            if (left.end != right.end) return left.end < right.end;
            return left.virtual_register < right.virtual_register;
        });

    std::vector<ReusableSlot> active_slots;
    std::vector<std::uint32_t> free_slots;
    std::uint32_t slot_count = 0;
    for (const auto& interval : spilled_intervals) {
        for (auto iterator = active_slots.begin(); iterator != active_slots.end();) {
            if (iterator->end < interval.start) {
                free_slots.push_back(iterator->index);
                iterator = active_slots.erase(iterator);
            } else ++iterator;
        }
        std::uint32_t slot = 0;
        if (!free_slots.empty()) {
            slot = free_slots.back();
            free_slots.pop_back();
            ++allocation.reused_spill_slot_count;
        } else {
            slot = slot_count++;
        }
        allocation.locations[interval.virtual_register].stack_offset =
            -static_cast<std::int32_t>(function.local_stack_size + (slot + 1U) * 8U);
        active_slots.push_back({interval.end, slot});
    }

    allocation.spill_slot_count = slot_count;
    allocation.frame_size_before_slot_reuse = align_frame(function.local_stack_size + allocation.spill_count * 8U);
    allocation.frame_size = align_frame(function.local_stack_size + slot_count * 8U);
    allocation.frame_bytes_saved = allocation.frame_size_before_slot_reuse - allocation.frame_size;
    return allocation;
}

StackAllocation allocate_stack_slots(const Function& function) {
    StackAllocation allocation;
    if (function.register_count > 16384U) {
        allocation.diagnostics.push_back({DiagnosticSeverity::error,
            "baseline stack allocator virtual-register limit exceeded in @" + function.name, {}});
        return allocation;
    }

    allocation.offsets.reserve(function.register_count);
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
        const auto byte_offset = static_cast<std::uint64_t>(reg + 1U) * 8U;
        allocation.offsets.push_back(-static_cast<std::int32_t>(function.local_stack_size + byte_offset));
    }
    allocation.frame_size = align_frame(function.local_stack_size + static_cast<std::uint32_t>(function.register_count) * 8U);
    return allocation;
}

} // namespace forge::machine
