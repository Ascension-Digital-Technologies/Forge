// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

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



bool is_call_opcode(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::call_i32: case Opcode::call_i64: case Opcode::call_f32: case Opcode::call_f64:
    case Opcode::call_void: case Opcode::call_aggregate: case Opcode::call_indirect_i32: case Opcode::call_indirect_i64:
    case Opcode::call_indirect_f32: case Opcode::call_indirect_f64: case Opcode::call_indirect_void:
        return true;
    default:
        return false;
    }
}

Opcode split_store_opcode(RegisterClass register_class, std::uint8_t width) noexcept {
    if (register_class == RegisterClass::floating)
        return width == 8U ? Opcode::store_stack_f64 : Opcode::store_stack_f32;
    if (width <= 1U) return Opcode::store_stack_i8;
    if (width <= 2U) return Opcode::store_stack_i16;
    if (width <= 4U) return Opcode::store_stack_i32;
    return Opcode::store_stack_i64;
}

Opcode split_load_opcode(RegisterClass register_class, std::uint8_t width) noexcept {
    if (register_class == RegisterClass::floating)
        return width == 8U ? Opcode::load_stack_f64 : Opcode::load_stack_f32;
    if (width <= 1U) return Opcode::load_stack_i8;
    if (width <= 2U) return Opcode::load_stack_i16;
    if (width <= 4U) return Opcode::load_stack_i32;
    return Opcode::load_stack_i64;
}

void rewrite_register(Instruction& instruction, VirtualRegister from, VirtualRegister to) {
    for (auto& input : instruction.inputs)
        if (input == from) input = to;
    for (auto& successor : instruction.successors)
        for (auto& argument : successor.arguments)
            if (argument == from) argument = to;
}

bool produces_result(Opcode opcode) {
    switch (opcode) {
    case Opcode::store_stack_i8: case Opcode::store_stack_i16: case Opcode::store_stack_i32:
    case Opcode::store_stack_i64: case Opcode::store_stack_f32: case Opcode::store_stack_f64:
    case Opcode::store_ptr_i8: case Opcode::store_ptr_i16: case Opcode::store_ptr_i32:
    case Opcode::store_ptr_i64: case Opcode::store_ptr_f32: case Opcode::store_ptr_f64:
    case Opcode::call_void: case Opcode::call_aggregate: case Opcode::call_indirect_void:
    case Opcode::jump: case Opcode::branch_i1:
    case Opcode::return_i32: case Opcode::return_i64: case Opcode::return_f32:
    case Opcode::return_f64: case Opcode::return_void: case Opcode::return_aggregate:
        return false;
    default:
        return true;
    }
}
} // namespace

LiveRangeSplitStats split_live_ranges_around_calls(Function& function) {
    LiveRangeSplitStats stats;
    if (function.blocks.empty() || function.register_count == 0U) return stats;

    // Split only where the current ABI model has no cheap register solution:
    // floating values live across a call and integer pressure above the two
    // available callee-saved registers. The transformation is deliberately
    // restricted to values whose remaining uses stay in the same block, which
    // makes the rename dominance-exact without introducing edge copies.
    bool changed = true;
    while (changed) {
        changed = false;
        const auto liveness = analyze_liveness(function);
        const auto intervals = compute_live_intervals(function);
        for (std::size_t block_index = 0; block_index < function.blocks.size() && !changed; ++block_index) {
            auto& block = function.blocks[block_index];
            for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
                const auto& call = block.instructions[instruction_index];
                if (!is_call_opcode(call.opcode)) continue;

                std::vector<VirtualRegister> floating_candidates;
                std::vector<VirtualRegister> integer_candidates;
                for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
                    if (!liveness.live_after[block_index][instruction_index][reg] ||
                        liveness.live_out[block_index][reg] || reg == call.result) continue;
                    bool used_after = false;
                    for (std::size_t later = instruction_index + 1U; later < block.instructions.size() && !used_after; ++later) {
                        const auto& instruction = block.instructions[later];
                        used_after = std::find(instruction.inputs.begin(), instruction.inputs.end(), reg) != instruction.inputs.end();
                        if (!used_after) {
                            for (const auto& successor : instruction.successors) {
                                if (std::find(successor.arguments.begin(), successor.arguments.end(), reg) != successor.arguments.end()) {
                                    used_after = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!used_after) continue;
                    const bool floating = reg < function.register_classes.size() &&
                                          function.register_classes[reg] == RegisterClass::floating;
                    (floating ? floating_candidates : integer_candidates).push_back(reg);
                }

                std::stable_sort(integer_candidates.begin(), integer_candidates.end(), [&](VirtualRegister left, VirtualRegister right) {
                    if (intervals[left].spill_weight != intervals[right].spill_weight)
                        return intervals[left].spill_weight < intervals[right].spill_weight;
                    return left < right;
                });
                std::vector<VirtualRegister> selected = floating_candidates;
                if (integer_candidates.size() > 2U)
                    selected.insert(selected.end(), integer_candidates.begin(), integer_candidates.end() - 2);
                if (selected.empty()) continue;

                std::vector<Instruction> stores;
                std::vector<Instruction> loads;
                stores.reserve(selected.size());
                loads.reserve(selected.size());
                std::vector<std::pair<VirtualRegister, VirtualRegister>> replacements;
                replacements.reserve(selected.size());
                for (const auto reg : selected) {
                    const auto width = reg < function.register_widths.size() ? function.register_widths[reg] : 64U;
                    const auto register_class = reg < function.register_classes.size()
                        ? function.register_classes[reg] : RegisterClass::integer;
                    function.local_stack_size += 8U;
                    const auto offset = -static_cast<std::int64_t>(function.local_stack_size);
                    stores.push_back({split_store_opcode(register_class, width), 0U, {reg}, offset, 0U, {}, {}});
                    const auto replacement = function.register_count++;
                    function.register_widths.push_back(width);
                    function.register_classes.push_back(register_class);
                    loads.push_back({split_load_opcode(register_class, width), replacement, {}, offset, 0U, {}, {}});
                    replacements.emplace_back(reg, replacement);
                    ++stats.split_values;
                    ++stats.transition_stores;
                    ++stats.transition_loads;
                    stats.transition_bytes += 16U;
                }

                block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(instruction_index),
                                          stores.begin(), stores.end());
                instruction_index += stores.size();
                block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(instruction_index + 1U),
                                          loads.begin(), loads.end());
                const auto rewrite_start = instruction_index + 1U + loads.size();
                for (std::size_t later = rewrite_start; later < block.instructions.size(); ++later)
                    for (const auto& [from, to] : replacements)
                        rewrite_register(block.instructions[later], from, to);
                changed = true;
                break;
            }
        }
    }
    // Extend splitting across a simple CFG continuation. This handles the
    // common call-at-end-of-block shape when the continuation block has a
    // single predecessor, so the reload dominates every rewritten use without
    // requiring critical-edge copies or phi repair.
    bool cross_block_changed = true;
    while (cross_block_changed) {
        cross_block_changed = false;
        const auto liveness = analyze_liveness(function);
        const auto intervals = compute_live_intervals(function);
        std::unordered_map<std::string, std::size_t> block_indices;
        std::vector<std::uint32_t> predecessor_counts(function.blocks.size(), 0U);
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            block_indices.emplace(function.blocks[index].name, index);
        for (const auto& block : function.blocks) {
            if (block.instructions.empty()) continue;
            for (const auto& successor : block.instructions.back().successors) {
                const auto target = block_indices.find(successor.block);
                if (target != block_indices.end()) ++predecessor_counts[target->second];
            }
        }

        for (std::size_t block_index = 0; block_index < function.blocks.size() && !cross_block_changed; ++block_index) {
            auto& block = function.blocks[block_index];
            if (block.instructions.empty() || block.instructions.back().successors.size() != 1U) continue;
            const auto successor_it = block_indices.find(block.instructions.back().successors.front().block);
            if (successor_it == block_indices.end() || predecessor_counts[successor_it->second] != 1U) continue;
            const auto successor_index = successor_it->second;

            for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
                const auto& call = block.instructions[instruction_index];
                if (!is_call_opcode(call.opcode)) continue;

                std::vector<VirtualRegister> floating_candidates;
                std::vector<VirtualRegister> integer_candidates;
                for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
                    if (!liveness.live_after[block_index][instruction_index][reg] ||
                        !liveness.live_out[block_index][reg] || reg == call.result) continue;
                    bool used_later_in_block = false;
                    for (std::size_t later = instruction_index + 1U; later < block.instructions.size(); ++later) {
                        const auto& instruction = block.instructions[later];
                        used_later_in_block = std::find(instruction.inputs.begin(), instruction.inputs.end(), reg) != instruction.inputs.end();
                        if (!used_later_in_block) {
                            for (const auto& edge : instruction.successors) {
                                if (std::find(edge.arguments.begin(), edge.arguments.end(), reg) != edge.arguments.end()) {
                                    used_later_in_block = true;
                                    break;
                                }
                            }
                        }
                        if (used_later_in_block) break;
                    }
                    if (used_later_in_block) continue;

                    bool used_in_successor = false;
                    for (const auto& instruction : function.blocks[successor_index].instructions) {
                        if (std::find(instruction.inputs.begin(), instruction.inputs.end(), reg) != instruction.inputs.end()) {
                            used_in_successor = true;
                            break;
                        }
                        for (const auto& edge : instruction.successors) {
                            if (std::find(edge.arguments.begin(), edge.arguments.end(), reg) != edge.arguments.end()) {
                                used_in_successor = true;
                                break;
                            }
                        }
                        if (used_in_successor) break;
                    }
                    if (!used_in_successor) continue;
                    const bool floating = reg < function.register_classes.size() &&
                                          function.register_classes[reg] == RegisterClass::floating;
                    (floating ? floating_candidates : integer_candidates).push_back(reg);
                }

                std::stable_sort(integer_candidates.begin(), integer_candidates.end(), [&](VirtualRegister left, VirtualRegister right) {
                    if (intervals[left].spill_weight != intervals[right].spill_weight)
                        return intervals[left].spill_weight < intervals[right].spill_weight;
                    return left < right;
                });
                std::vector<VirtualRegister> selected = floating_candidates;
                if (integer_candidates.size() > 2U)
                    selected.insert(selected.end(), integer_candidates.begin(), integer_candidates.end() - 2);
                if (selected.empty()) continue;

                std::vector<Instruction> stores;
                std::vector<Instruction> loads;
                std::vector<std::pair<VirtualRegister, VirtualRegister>> replacements;
                for (const auto reg : selected) {
                    const auto width = reg < function.register_widths.size() ? function.register_widths[reg] : 64U;
                    const auto register_class = reg < function.register_classes.size()
                        ? function.register_classes[reg] : RegisterClass::integer;
                    function.local_stack_size += 8U;
                    const auto offset = -static_cast<std::int64_t>(function.local_stack_size);
                    stores.push_back({split_store_opcode(register_class, width), 0U, {reg}, offset, 0U, {}, {}});
                    const auto replacement = function.register_count++;
                    function.register_widths.push_back(width);
                    function.register_classes.push_back(register_class);
                    loads.push_back({split_load_opcode(register_class, width), replacement, {}, offset, 0U, {}, {}});
                    replacements.emplace_back(reg, replacement);
                    ++stats.split_values;
                    ++stats.cross_block_split_values;
                    ++stats.transition_stores;
                    ++stats.transition_loads;
                    stats.transition_bytes += 16U;
                }

                block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(instruction_index),
                                          stores.begin(), stores.end());
                auto& successor_block = function.blocks[successor_index];
                successor_block.instructions.insert(successor_block.instructions.begin(), loads.begin(), loads.end());
                for (std::size_t later = loads.size(); later < successor_block.instructions.size(); ++later)
                    for (const auto& [from, to] : replacements)
                        rewrite_register(successor_block.instructions[later], from, to);
                cross_block_changed = true;
                break;
            }
        }
    }


    // Extend splitting through multi-predecessor continuations by introducing
    // an explicit edge block and a block parameter in the merge block. Every
    // predecessor supplies either the original value or the post-call reload,
    // which gives the renamed continuation a proper SSA merge point.
    bool critical_edge_changed = true;
    std::uint32_t edge_serial = 0U;
    while (critical_edge_changed) {
        critical_edge_changed = false;
        const auto liveness = analyze_liveness(function);
        const auto intervals = compute_live_intervals(function);
        std::unordered_map<std::string, std::size_t> block_indices;
        std::vector<std::uint32_t> predecessor_counts(function.blocks.size(), 0U);
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            block_indices.emplace(function.blocks[index].name, index);
        for (const auto& candidate_block : function.blocks) {
            if (candidate_block.instructions.empty()) continue;
            for (const auto& successor : candidate_block.instructions.back().successors) {
                const auto target = block_indices.find(successor.block);
                if (target != block_indices.end()) ++predecessor_counts[target->second];
            }
        }

        for (std::size_t block_index = 0; block_index < function.blocks.size() && !critical_edge_changed; ++block_index) {
            if (function.blocks[block_index].instructions.empty() ||
                function.blocks[block_index].instructions.back().successors.size() != 1U) continue;
            const auto successor_name = function.blocks[block_index].instructions.back().successors.front().block;
            const auto successor_it = block_indices.find(successor_name);
            if (successor_it == block_indices.end() || predecessor_counts[successor_it->second] <= 1U) continue;
            const auto successor_index = successor_it->second;

            for (std::size_t instruction_index = 0;
                 instruction_index < function.blocks[block_index].instructions.size(); ++instruction_index) {
                const auto& call = function.blocks[block_index].instructions[instruction_index];
                if (!is_call_opcode(call.opcode)) continue;

                std::vector<VirtualRegister> floating_candidates;
                std::vector<VirtualRegister> integer_candidates;
                for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
                    if (!liveness.live_after[block_index][instruction_index][reg] ||
                        !liveness.live_out[block_index][reg] || reg == call.result) continue;
                    bool used_later_in_block = false;
                    for (std::size_t later = instruction_index + 1U;
                         later < function.blocks[block_index].instructions.size(); ++later) {
                        const auto& instruction = function.blocks[block_index].instructions[later];
                        if (std::find(instruction.inputs.begin(), instruction.inputs.end(), reg) != instruction.inputs.end()) {
                            used_later_in_block = true;
                            break;
                        }
                        for (const auto& edge : instruction.successors) {
                            if (std::find(edge.arguments.begin(), edge.arguments.end(), reg) != edge.arguments.end()) {
                                used_later_in_block = true;
                                break;
                            }
                        }
                        if (used_later_in_block) break;
                    }
                    if (used_later_in_block) continue;

                    bool used_in_successor = false;
                    for (const auto& instruction : function.blocks[successor_index].instructions) {
                        if (std::find(instruction.inputs.begin(), instruction.inputs.end(), reg) != instruction.inputs.end()) {
                            used_in_successor = true;
                            break;
                        }
                        for (const auto& edge : instruction.successors) {
                            if (std::find(edge.arguments.begin(), edge.arguments.end(), reg) != edge.arguments.end()) {
                                used_in_successor = true;
                                break;
                            }
                        }
                        if (used_in_successor) break;
                    }
                    if (!used_in_successor) continue;
                    const bool floating = reg < function.register_classes.size() &&
                                          function.register_classes[reg] == RegisterClass::floating;
                    (floating ? floating_candidates : integer_candidates).push_back(reg);
                }

                std::stable_sort(integer_candidates.begin(), integer_candidates.end(), [&](VirtualRegister left, VirtualRegister right) {
                    if (intervals[left].spill_weight != intervals[right].spill_weight)
                        return intervals[left].spill_weight < intervals[right].spill_weight;
                    return left < right;
                });
                std::vector<VirtualRegister> selected = floating_candidates;
                if (integer_candidates.size() > 2U)
                    selected.insert(selected.end(), integer_candidates.begin(), integer_candidates.end() - 2);
                if (selected.empty()) continue;

                std::vector<Instruction> stores;
                std::vector<Instruction> reloads;
                std::vector<std::pair<VirtualRegister, VirtualRegister>> merge_replacements;
                std::vector<VirtualRegister> reloaded_values;
                for (const auto reg : selected) {
                    const auto width = reg < function.register_widths.size() ? function.register_widths[reg] : 64U;
                    const auto register_class = reg < function.register_classes.size()
                        ? function.register_classes[reg] : RegisterClass::integer;
                    function.local_stack_size += 8U;
                    const auto offset = -static_cast<std::int64_t>(function.local_stack_size);
                    stores.push_back({split_store_opcode(register_class, width), 0U, {reg}, offset, 0U, {}, {}});

                    const auto reloaded = function.register_count++;
                    function.register_widths.push_back(width);
                    function.register_classes.push_back(register_class);
                    reloads.push_back({split_load_opcode(register_class, width), reloaded, {}, offset, 0U, {}, {}});
                    reloaded_values.push_back(reloaded);

                    const auto merged = function.register_count++;
                    function.register_widths.push_back(width);
                    function.register_classes.push_back(register_class);
                    merge_replacements.emplace_back(reg, merged);
                    function.blocks[successor_index].parameters.push_back(merged);

                    ++stats.split_values;
                    ++stats.cross_block_split_values;
                    ++stats.critical_edge_split_values;
                    ++stats.merge_parameters;
                    ++stats.transition_stores;
                    ++stats.transition_loads;
                    stats.transition_bytes += 16U;
                }

                auto& source_block = function.blocks[block_index];
                source_block.instructions.insert(
                    source_block.instructions.begin() + static_cast<std::ptrdiff_t>(instruction_index),
                    stores.begin(), stores.end());

                const auto edge_name = source_block.name + ".split." + std::to_string(edge_serial++);
                source_block.instructions.back().successors.front().block = edge_name;

                // All existing incoming edges to the merge block pass the
                // original values. The newly-created split edge passes reloads.
                for (auto& incoming_block : function.blocks) {
                    if (incoming_block.instructions.empty()) continue;
                    for (auto& incoming : incoming_block.instructions.back().successors) {
                        if (incoming.block != successor_name) continue;
                        for (const auto reg : selected) incoming.arguments.push_back(reg);
                    }
                }

                for (const auto& [from, to] : merge_replacements) {
                    for (auto& instruction : function.blocks[successor_index].instructions)
                        rewrite_register(instruction, from, to);
                }

                Block edge_block;
                edge_block.name = edge_name;
                edge_block.instructions = std::move(reloads);
                Instruction jump{Opcode::jump, 0U, {}, 0, 0U, {}, {}};
                jump.successors.push_back({successor_name, reloaded_values});
                edge_block.instructions.push_back(std::move(jump));
                function.blocks.push_back(std::move(edge_block));
                ++stats.critical_edge_blocks;
                critical_edge_changed = true;
                break;
            }
        }
    }

    return stats;
}

std::vector<LiveInterval> compute_live_intervals(const Function& function) {
    std::vector<LiveInterval> intervals(function.register_count);
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg)
        intervals[reg] = {reg, undefined_position, 0, 0, 0, 0, 0, {}};

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

    // Preserve liveness as disjoint per-block segments instead of only one
    // bounding range. This exposes holes created by interleaved mutually
    // exclusive CFG paths and allows the allocator to recover false spills.
    for (std::size_t block_index = 0; block_index < block_count; ++block_index) {
        const auto& block = function.blocks[block_index];
        std::vector<std::uint32_t> first(function.register_count, undefined_position);
        std::vector<std::uint32_t> last(function.register_count, 0U);
        const auto mark = [&](VirtualRegister reg, std::uint32_t at) {
            if (reg >= function.register_count) return;
            first[reg] = std::min(first[reg], at);
            last[reg] = std::max(last[reg], at);
        };
        for (VirtualRegister reg = 0; reg < function.register_count; ++reg)
            if (live_in[block_index][reg]) mark(reg, block_starts[block_index]);
        for (const auto parameter : block.parameters) mark(parameter, block_starts[block_index]);
        auto at = block_starts[block_index] + 1U;
        for (const auto& instruction : block.instructions) {
            for (const auto input : instruction.inputs) mark(input, at);
            for (const auto& successor : instruction.successors)
                for (const auto argument : successor.arguments) mark(argument, at);
            if (produces_result(instruction.opcode)) mark(instruction.result, at);
            ++at;
        }
        for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
            if (live_out[block_index][reg]) mark(reg, block_ends[block_index]);
            if (first[reg] != undefined_position)
                intervals[reg].segments.push_back({first[reg], last[reg]});
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

bool segments_overlap(const LiveInterval& left, const LiveInterval& right) {
    for (const auto& lhs : left.segments)
        for (const auto& rhs : right.segments)
            if (lhs.start <= rhs.end && rhs.start <= lhs.end) return true;
    return false;
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
            case Opcode::call_i32: case Opcode::call_i64: case Opcode::call_f32: case Opcode::call_f64: case Opcode::call_void: case Opcode::call_aggregate:
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

    // Measure pressure from disjoint liveness segments rather than bounding
    // intervals. Interleaved blocks on mutually exclusive CFG paths therefore
    // no longer inflate pressure or force unnecessary spills.
    for (const auto& interval : allocation.intervals) {
        if (interval.segments.size() > 1U) {
            ++allocation.segmented_interval_count;
            allocation.live_range_hole_count += static_cast<std::uint32_t>(interval.segments.size() - 1U);
        }
    }
    for (VirtualRegister left = 0; left < function.register_count; ++left) {
        if (allocation.intervals[left].use_count == 0U) continue;
        for (VirtualRegister right = left + 1U; right < function.register_count; ++right) {
            if (allocation.intervals[right].use_count == 0U) continue;
            const bool left_floating = left < function.register_classes.size() &&
                                       function.register_classes[left] == RegisterClass::floating;
            const bool right_floating = right < function.register_classes.size() &&
                                        function.register_classes[right] == RegisterClass::floating;
            if (left_floating == right_floating && segments_overlap(allocation.intervals[left], allocation.intervals[right]))
                ++allocation.interference_edge_count;
        }
    }
    for (std::uint32_t scan = 0; scan <= position; ++scan) {
        std::uint32_t integer_pressure = 0;
        std::uint32_t floating_pressure = 0;
        for (const auto& interval : allocation.intervals) {
            if (interval.use_count == 0U) continue;
            const bool live = std::any_of(interval.segments.begin(), interval.segments.end(),
                [&](const LiveSegment& segment) { return segment.start <= scan && scan <= segment.end; });
            if (!live) continue;
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

    // Recover registers for spills caused only by bounding-range overlap. A
    // physical register can be shared when no already allocated value of the
    // same class interferes in any real liveness segment. This is the first
    // segmented-allocation stage and requires no mid-interval location change.
    const auto integer_available = [&](VirtualRegister reg, PhysicalRegister physical) {
        for (VirtualRegister other = 0; other < function.register_count; ++other) {
            if (other == reg) continue;
            const auto& location = allocation.locations[other];
            if (location.kind != LocationKind::physical_register || location.physical != physical) continue;
            if (segments_overlap(allocation.intervals[reg], allocation.intervals[other])) return false;
        }
        return true;
    };
    const auto floating_available = [&](VirtualRegister reg, FloatingPhysicalRegister physical) {
        for (VirtualRegister other = 0; other < function.register_count; ++other) {
            if (other == reg) continue;
            const auto& location = allocation.locations[other];
            if (location.kind != LocationKind::floating_register || location.floating != physical) continue;
            if (segments_overlap(allocation.intervals[reg], allocation.intervals[other])) return false;
        }
        return true;
    };
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
        if (allocation.locations[reg].kind != LocationKind::stack_slot || allocation.intervals[reg].use_count == 0U)
            continue;
        const bool floating = reg < function.register_classes.size() &&
                              function.register_classes[reg] == RegisterClass::floating;
        bool recovered = false;
        if (floating) {
            if (!crosses_call(allocation.intervals[reg]) && !forced_floating_spill[reg]) {
                for (const auto physical : floating_physicals) {
                    if (!floating_available(reg, physical)) continue;
                    allocation.locations[reg] = {LocationKind::floating_register, PhysicalRegister::r10d, physical, 0};
                    recovered = true;
                    break;
                }
            }
        } else {
            for (const auto physical : integer_physicals) {
                if (crosses_call(allocation.intervals[reg]) && !is_callee_saved(physical)) continue;
                if (!integer_available(reg, physical)) continue;
                allocation.locations[reg] = {LocationKind::physical_register, physical, FloatingPhysicalRegister::xmm2, 0};
                record_integer_allocation(physical);
                recovered = true;
                break;
            }
        }
        if (recovered) {
            --allocation.spill_count;
            ++allocation.physical_count;
            ++allocation.hole_aware_register_reuse_count;
        }
    }


    // Honor copy affinities after the initial scan and hole-aware recovery.
    // Local coalescing above handles adjacent intervals. This global stage uses
    // segmented interference, so copies that cross block boundaries or
    // liveness holes can still share the source location when no real live
    // segment conflicts with that physical register.
    const auto copy_segments_conflict = [](const LiveInterval& source, const LiveInterval& destination) {
        for (const auto& left : source.segments) {
            for (const auto& right : destination.segments) {
                const auto overlap_start = std::max(left.start, right.start);
                const auto overlap_end = std::min(left.end, right.end);
                if (overlap_start < overlap_end) return true;
            }
        }
        return false;
    };
    for (VirtualRegister destination = 0; destination < function.register_count; ++destination) {
        const auto source = copy_sources[destination];
        if (source >= function.register_count || source == destination) continue;
        if (allocation.intervals[destination].use_count == 0U ||
            allocation.intervals[source].use_count == 0U) continue;
        if (copy_segments_conflict(allocation.intervals[source], allocation.intervals[destination])) continue;

        const bool floating = destination < function.register_classes.size() &&
                              function.register_classes[destination] == RegisterClass::floating;
        const bool source_floating = source < function.register_classes.size() &&
                                     function.register_classes[source] == RegisterClass::floating;
        if (floating != source_floating) continue;

        auto& destination_location = allocation.locations[destination];
        const auto& source_location = allocation.locations[source];
        const bool destination_was_spilled = destination_location.kind == LocationKind::stack_slot;
        bool coalesced = false;
        if (floating && source_location.kind == LocationKind::floating_register &&
            !crosses_call(allocation.intervals[destination]) && !forced_floating_spill[destination] &&
            floating_available(destination, source_location.floating)) {
            destination_location = {LocationKind::floating_register, PhysicalRegister::r10d,
                                    source_location.floating, 0};
            coalesced = true;
        } else if (!floating && source_location.kind == LocationKind::physical_register &&
                   (!crosses_call(allocation.intervals[destination]) ||
                    is_callee_saved(source_location.physical)) &&
                   integer_available(destination, source_location.physical)) {
            destination_location = {LocationKind::physical_register, source_location.physical,
                                    FloatingPhysicalRegister::xmm2, 0};
            if (destination_was_spilled) record_integer_allocation(source_location.physical);
            coalesced = true;
        }
        if (!coalesced) continue;
        ++allocation.global_copy_affinity_count;
        ++allocation.coalesced_copy_count;
        if (destination_was_spilled) {
            --allocation.spill_count;
            ++allocation.physical_count;
            ++allocation.copy_spills_recovered;
        }
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
