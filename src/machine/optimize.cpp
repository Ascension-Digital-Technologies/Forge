// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include "forge/machine/optimize.hpp"
#include "forge/machine/liveness.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace forge::machine {

SlpCostModel SlpCostModel::x86_64(X86VectorIsa isa) noexcept {
    SlpCostModel model;
    model.sse2 = true;
    model.vector_bits = 128;
    model.backend_vector_bits = isa == X86VectorIsa::avx512 ? 512 :
        (isa == X86VectorIsa::avx2 ? 256 : 128);
    model.vector_register_budget = 12;

    switch (isa) {
    case X86VectorIsa::sse2:
        break;
    case X86VectorIsa::sse41:
        model.sse41 = true;
        model.shuffle_cost = 0.70;
        break;
    case X86VectorIsa::avx:
        model.sse41 = true;
        model.avx = true;
        // AVX1 does not widen packed integer arithmetic beyond 128 bits.
        model.vector_bits = 128;
        model.shuffle_cost = 0.65;
        model.vector_setup_cost = 0.16;
        break;
    case X86VectorIsa::avx2:
        model.sse41 = true;
        model.avx = true;
        model.avx2 = true;
        model.vector_bits = 256;
        model.vector_integer_throughput = 0.33;
        model.vector_memory_cost = 0.85;
        model.broadcast_cost = 0.40;
        model.shuffle_cost = 0.55;
        model.vector_setup_cost = 0.14;
        break;
    case X86VectorIsa::avx512:
        model.sse41 = true;
        model.avx = true;
        model.avx2 = true;
        model.avx512f = true;
        model.avx512bw = true;
        model.avx512vl = true;
        model.vector_bits = 512;
        model.vector_register_budget = 28;
        model.mask_register_budget = 7;
        model.vector_integer_throughput = 0.30;
        model.vector_memory_cost = 0.82;
        model.broadcast_cost = 0.35;
        model.shuffle_cost = 0.50;
        model.vector_setup_cost = 0.12;
        break;
    }
    return model;
}

namespace {


bool has_result(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::store_stack_i8: case Opcode::store_stack_i16: case Opcode::store_stack_i32:
    case Opcode::store_stack_i64: case Opcode::store_stack_f32: case Opcode::store_stack_f64:
    case Opcode::store_ptr_i8: case Opcode::store_ptr_i16: case Opcode::store_ptr_i32:
    case Opcode::store_ptr_i64: case Opcode::store_ptr_f32: case Opcode::store_ptr_f64:
    case Opcode::add_i64_contiguous_inplace:
    case Opcode::binary_i32_contiguous_inplace:
    case Opcode::binary_i64_contiguous_inplace:
    case Opcode::binary_i32_contiguous_map:
    case Opcode::binary_i64_contiguous_map:
    case Opcode::binary_i32_contiguous_map2:
    case Opcode::binary_i64_contiguous_map2:
    case Opcode::binary_i32_contiguous_map3:
    case Opcode::binary_i64_contiguous_map3:
    case Opcode::binary_i32_contiguous_chain:
    case Opcode::binary_i64_contiguous_chain:
    case Opcode::binary_i32_contiguous_dag:
    case Opcode::binary_i64_contiguous_dag:
    case Opcode::binary_i32_contiguous_dag_reuse:
    case Opcode::binary_i64_contiguous_dag_reuse:
    case Opcode::call_void: case Opcode::call_aggregate: case Opcode::call_indirect_void:
    case Opcode::jump: case Opcode::branch_i1:
    case Opcode::return_i32: case Opcode::return_i64: case Opcode::return_f32:
    case Opcode::return_f64: case Opcode::return_void: case Opcode::return_aggregate:
        return false;
    default:
        return true;
    }
}



bool is_removable_when_dead(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::store_stack_i8: case Opcode::store_stack_i16: case Opcode::store_stack_i32:
    case Opcode::store_stack_i64: case Opcode::store_stack_f32: case Opcode::store_stack_f64:
    case Opcode::store_ptr_i8: case Opcode::store_ptr_i16: case Opcode::store_ptr_i32:
    case Opcode::store_ptr_i64: case Opcode::store_ptr_f32: case Opcode::store_ptr_f64:
    case Opcode::add_i64_contiguous_inplace:
    case Opcode::binary_i32_contiguous_inplace:
    case Opcode::binary_i64_contiguous_inplace:
    case Opcode::binary_i32_contiguous_map:
    case Opcode::binary_i64_contiguous_map:
    case Opcode::binary_i32_contiguous_map2:
    case Opcode::binary_i64_contiguous_map2:
    case Opcode::binary_i32_contiguous_map3:
    case Opcode::binary_i64_contiguous_map3:
    case Opcode::binary_i32_contiguous_chain:
    case Opcode::binary_i64_contiguous_chain:
    case Opcode::binary_i32_contiguous_dag:
    case Opcode::binary_i64_contiguous_dag:
    case Opcode::binary_i32_contiguous_dag_reuse:
    case Opcode::binary_i64_contiguous_dag_reuse:
    case Opcode::load_ptr_i8: case Opcode::load_ptr_i16: case Opcode::load_ptr_i32:
    case Opcode::load_ptr_i64: case Opcode::load_ptr_f32: case Opcode::load_ptr_f64:
    case Opcode::call_i32: case Opcode::call_i64: case Opcode::call_f32: case Opcode::call_f64:
    case Opcode::call_void: case Opcode::call_aggregate: case Opcode::call_indirect_i32: case Opcode::call_indirect_i64:
    case Opcode::call_indirect_f32: case Opcode::call_indirect_f64: case Opcode::call_indirect_void:
    case Opcode::div_s_i32: case Opcode::div_s_i64: case Opcode::div_u_i32: case Opcode::div_u_i64:
    case Opcode::rem_s_i32: case Opcode::rem_s_i64: case Opcode::rem_u_i32: case Opcode::rem_u_i64:
    case Opcode::jump: case Opcode::branch_i1:
    case Opcode::return_i32: case Opcode::return_i64: case Opcode::return_f32:
    case Opcode::return_f64: case Opcode::return_void: case Opcode::return_aggregate:
        return false;
    default:
        return true;
    }
}

bool is_comparison(Opcode opcode) noexcept {
    return (opcode >= Opcode::cmp_eq_f32 && opcode <= Opcode::cmp_ge_f64) ||
           (opcode >= Opcode::cmp_eq_i32 && opcode <= Opcode::cmp_uge_i64);
}

bool is_integer_comparison(Opcode opcode) noexcept {
    return opcode >= Opcode::cmp_eq_i32 && opcode <= Opcode::cmp_uge_i64;
}

bool is_floating_relational_comparison(Opcode opcode) noexcept {
    return opcode == Opcode::cmp_lt_f32 || opcode == Opcode::cmp_le_f32 ||
           opcode == Opcode::cmp_gt_f32 || opcode == Opcode::cmp_ge_f32 ||
           opcode == Opcode::cmp_lt_f64 || opcode == Opcode::cmp_le_f64 ||
           opcode == Opcode::cmp_gt_f64 || opcode == Opcode::cmp_ge_f64;
}

bool is_copy(Opcode opcode) noexcept {
    return opcode == Opcode::copy || opcode == Opcode::copy_f32 || opcode == Opcode::copy_f64;
}

bool is_redundant_cast(const Instruction& instruction) noexcept {
    if (instruction.opcode != Opcode::zero_extend && instruction.opcode != Opcode::sign_extend &&
        instruction.opcode != Opcode::truncate) return false;
    const auto source_bits = static_cast<unsigned>((instruction.immediate >> 8U) & 0xffU);
    const auto result_bits = static_cast<unsigned>(instruction.immediate & 0xffU);
    return source_bits != 0U && source_bits == result_bits;
}

VirtualRegister resolve_alias(std::vector<VirtualRegister>& aliases, VirtualRegister reg) {
    if (reg >= aliases.size()) return reg;
    auto root = reg;
    while (aliases[root] != root) root = aliases[root];
    while (aliases[reg] != reg) {
        const auto next = aliases[reg];
        aliases[reg] = root;
        reg = next;
    }
    return root;
}

bool compatible(const Function& function, VirtualRegister result, VirtualRegister source) noexcept {
    return result < function.register_count && source < function.register_count &&
           function.register_widths[result] == function.register_widths[source] &&
           function.register_classes[result] == function.register_classes[source];
}


bool supports_integer_immediate(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::add_i32: case Opcode::add_i64:
    case Opcode::sub_i32: case Opcode::sub_i64:
    case Opcode::mul_i32: case Opcode::mul_i64:
    case Opcode::and_i32: case Opcode::and_i64:
    case Opcode::or_i32: case Opcode::or_i64:
    case Opcode::xor_i32: case Opcode::xor_i64:
    case Opcode::shl_i32: case Opcode::shl_i64:
    case Opcode::shr_s_i32: case Opcode::shr_s_i64:
    case Opcode::shr_u_i32: case Opcode::shr_u_i64:
        return true;
    default:
        return false;
    }
}

bool is_commutative_integer(Opcode opcode) noexcept {
    return opcode == Opcode::add_i32 || opcode == Opcode::add_i64 ||
           opcode == Opcode::mul_i32 || opcode == Opcode::mul_i64 ||
           opcode == Opcode::and_i32 || opcode == Opcode::and_i64 ||
           opcode == Opcode::or_i32 || opcode == Opcode::or_i64 ||
           opcode == Opcode::xor_i32 || opcode == Opcode::xor_i64;
}

bool is_shift(Opcode opcode) noexcept {
    return opcode == Opcode::shl_i32 || opcode == Opcode::shl_i64 ||
           opcode == Opcode::shr_s_i32 || opcode == Opcode::shr_s_i64 ||
           opcode == Opcode::shr_u_i32 || opcode == Opcode::shr_u_i64;
}

bool is_pointer_memory_operation(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::load_ptr_i8: case Opcode::load_ptr_i16: case Opcode::load_ptr_i32:
    case Opcode::load_ptr_i64: case Opcode::load_ptr_f32: case Opcode::load_ptr_f64:
    case Opcode::store_ptr_i8: case Opcode::store_ptr_i16: case Opcode::store_ptr_i32:
    case Opcode::store_ptr_i64: case Opcode::store_ptr_f32: case Opcode::store_ptr_f64:
        return true;
    default:
        return false;
    }
}


struct SlpCandidateCost {
    std::size_t lane_bytes{};
    std::size_t lanes{};
    std::size_t operations_per_lane{1U};
    std::size_t loads_per_lane{1U};
    std::size_t stores_per_lane{1U};
    std::size_t vector_live_values{2U};
    Opcode primary_opcode{Opcode::add_i64};
    SlpMemoryPattern memory_pattern{SlpMemoryPattern::contiguous_unaligned};
    std::size_t broadcasts_per_chunk{};
    std::size_t shuffles_per_chunk{};
    double operation_mix_multiplier{1.0};
};

double slp_memory_multiplier(SlpMemoryPattern pattern, const SlpCostModel& model) noexcept {
    switch (pattern) {
    case SlpMemoryPattern::contiguous_aligned: return model.aligned_memory_multiplier;
    case SlpMemoryPattern::contiguous_unaligned: return model.unaligned_memory_multiplier;
    case SlpMemoryPattern::broadcast: return 1.0;
    case SlpMemoryPattern::interleaved: return model.interleaved_memory_multiplier;
    case SlpMemoryPattern::strided: return model.strided_memory_multiplier;
    case SlpMemoryPattern::gather_scatter: return model.gather_scatter_multiplier;
    }
    return 1.0;
}

double slp_operation_multiplier(Opcode opcode, const SlpCostModel& model) noexcept {
    switch (opcode) {
    case Opcode::and_i32: case Opcode::and_i64:
    case Opcode::or_i32: case Opcode::or_i64:
    case Opcode::xor_i32: case Opcode::xor_i64:
        return 0.80;
    case Opcode::add_i32: case Opcode::add_i64:
    case Opcode::sub_i32: case Opcode::sub_i64:
        return 1.00;
    case Opcode::mul_i32:
        return model.sse41 ? 1.35 : 2.25;
    case Opcode::mul_i64:
        // No native packed i64 multiply in SSE2/AVX2; treat it as expensive
        // unless a future backend teaches the vectorizer a lowering sequence.
        return model.avx512f ? 2.0 : 4.0;
    case Opcode::shl_i32: case Opcode::shl_i64:
    case Opcode::shr_s_i32: case Opcode::shr_s_i64:
    case Opcode::shr_u_i32: case Opcode::shr_u_i64:
        return 1.25;
    default:
        return 1.0;
    }
}

std::uint16_t slp_selected_vector_bits(std::size_t lane_bytes, std::size_t lanes, const SlpCostModel& model) noexcept {
    const auto total_bits = lane_bytes * lanes * 8U;
    const auto effective = static_cast<std::size_t>(model.effective_vector_bits());
    const auto usable = std::min(total_bits, effective);
    if (usable >= 512U) return 512U;
    if (usable >= 256U) return 256U;
    return 128U;
}

bool slp_profitable(const SlpCandidateCost& candidate, const SlpCostModel& model, OptimizationStats& stats) noexcept {
    ++stats.slp_candidates_considered;
    const auto effective_bits = model.effective_vector_bits();
    if (!model.vector_integer_available || !model.sse2 || effective_bits < 64U || candidate.lane_bytes == 0U || candidate.lanes < 2U) {
        ++stats.slp_candidates_rejected_target;
        return false;
    }
    const auto vector_bytes = static_cast<std::size_t>(effective_bits / 8U);
    if (vector_bytes < candidate.lane_bytes) {
        ++stats.slp_candidates_rejected_target;
        return false;
    }

    const auto total_bytes = candidate.lane_bytes * candidate.lanes;
    const auto chunks = (total_bytes + vector_bytes - 1U) / vector_bytes;
    const auto op_multiplier = slp_operation_multiplier(candidate.primary_opcode, model) * candidate.operation_mix_multiplier;
    const double scalar_op = (model.scalar_integer_latency + model.scalar_integer_throughput) * op_multiplier;
    const double vector_op = (model.vector_integer_latency + model.vector_integer_throughput) * op_multiplier;
    const double memory_multiplier = slp_memory_multiplier(candidate.memory_pattern, model);

    const double scalar = static_cast<double>(candidate.lanes) *
        (static_cast<double>(candidate.operations_per_lane) * scalar_op +
         static_cast<double>(candidate.loads_per_lane + candidate.stores_per_lane) * model.scalar_memory_cost);
    const double memory = static_cast<double>(chunks) *
        static_cast<double>(candidate.loads_per_lane + candidate.stores_per_lane) * model.vector_memory_cost * memory_multiplier;
    const double shuffle = static_cast<double>(chunks) *
        (static_cast<double>(candidate.broadcasts_per_chunk) * model.broadcast_cost +
         static_cast<double>(candidate.shuffles_per_chunk) * model.shuffle_cost);
    const double pressure = candidate.vector_live_values > model.vector_register_budget
        ? static_cast<double>(candidate.vector_live_values - model.vector_register_budget) * model.register_pressure_cost * static_cast<double>(chunks)
        : 0.0;
    const double vector = static_cast<double>(chunks) *
        (static_cast<double>(candidate.operations_per_lane) * vector_op + model.vector_setup_cost) + memory + shuffle + pressure;

    stats.slp_estimated_scalar_cost += scalar;
    stats.slp_estimated_vector_cost += vector;
    stats.slp_estimated_memory_cost += memory;
    stats.slp_estimated_shuffle_cost += shuffle;
    stats.slp_estimated_register_pressure_cost += pressure;

    if (!(vector * model.minimum_speedup < scalar)) {
        ++stats.slp_candidates_rejected_cost;
        return false;
    }
    ++stats.slp_candidates_selected;
    const auto selected_bits = slp_selected_vector_bits(candidate.lane_bytes, candidate.lanes, model);
    if (selected_bits >= 512U) ++stats.slp_width_512_selected;
    else if (selected_bits >= 256U) ++stats.slp_width_256_selected;
    else ++stats.slp_width_128_selected;
    return true;
}
std::size_t pointer_operand_index(Opcode opcode) noexcept {
    switch (opcode) {
    case Opcode::store_ptr_i8: case Opcode::store_ptr_i16: case Opcode::store_ptr_i32:
    case Opcode::store_ptr_i64: case Opcode::store_ptr_f32: case Opcode::store_ptr_f64:
        return 1U;
    default:
        return 0U;
    }
}

} // namespace

OptimizationStats optimize_function(Function& function, const SlpCostModel& slp_cost_model) {
    OptimizationStats stats;
    for (const auto& block : function.blocks)
        stats.instructions_before += static_cast<std::uint32_t>(block.instructions.size());

    // Record copy opportunities whose producer lives in another block. Machine SSA
    // guarantees a single definition, so the existing alias propagation can safely
    // rewrite these uses across block boundaries.
    std::vector<std::size_t> defining_block(function.register_count, function.blocks.size());
    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        for (const auto parameter : function.blocks[block_index].parameters)
            if (parameter < defining_block.size()) defining_block[parameter] = block_index;
        for (const auto& instruction : function.blocks[block_index].instructions)
            if (has_result(instruction.opcode) && instruction.result < defining_block.size())
                defining_block[instruction.result] = block_index;
    }
    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        for (const auto& instruction : function.blocks[block_index].instructions) {
            if (!is_copy(instruction.opcode) || instruction.inputs.size() != 1U ||
                instruction.result >= function.register_count) continue;
            const auto source = instruction.inputs.front();
            if (source < defining_block.size() && defining_block[source] != function.blocks.size() &&
                defining_block[source] != block_index && compatible(function, instruction.result, source))
                ++stats.cross_block_copies_propagated;
        }
    }

    // Thread parameterless forwarding blocks before instruction-level cleanup.
    // These blocks carry no values and contain only an unconditional jump, so
    // redirecting incoming edges preserves SSA edge semantics exactly.
    std::unordered_map<std::string, std::string> forwarding;
    for (std::size_t index = 1; index < function.blocks.size(); ++index) {
        const auto& block = function.blocks[index];
        if (!block.parameters.empty() || block.instructions.size() != 1U) continue;
        const auto& terminator = block.instructions.front();
        if (terminator.opcode != Opcode::jump || terminator.successors.size() != 1U ||
            !terminator.successors.front().arguments.empty() ||
            terminator.successors.front().block == block.name) continue;
        forwarding.emplace(block.name, terminator.successors.front().block);
    }
    const auto resolve_forwarding = [&](const std::string& initial) {
        std::string target = initial;
        std::unordered_set<std::string> seen;
        while (seen.insert(target).second) {
            const auto next = forwarding.find(target);
            if (next == forwarding.end()) break;
            target = next->second;
        }
        return target;
    };
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            for (auto& successor : instruction.successors) {
                const auto target = resolve_forwarding(successor.block);
                if (target != successor.block) {
                    successor.block = target;
                    ++stats.jump_threads;
                }
            }
        }
    }

    // Remove forwarding blocks that no longer have incoming edges.
    if (!forwarding.empty()) {
        std::unordered_set<std::string> referenced;
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
                for (const auto& successor : instruction.successors)
                    referenced.insert(successor.block);
        std::vector<Block> retained_blocks;
        retained_blocks.reserve(function.blocks.size());
        for (std::size_t index = 0; index < function.blocks.size(); ++index) {
            auto& block = function.blocks[index];
            if (index != 0U && forwarding.contains(block.name) && !referenced.contains(block.name)) {
                ++stats.empty_blocks_removed;
                continue;
            }
            retained_blocks.push_back(std::move(block));
        }
        function.blocks = std::move(retained_blocks);
    }

    // Delete unreachable blocks after threading. The first block is the entry.
    if (!function.blocks.empty()) {
        std::unordered_map<std::string, std::size_t> block_indices;
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            block_indices.emplace(function.blocks[index].name, index);
        std::vector<bool> reachable(function.blocks.size(), false);
        std::vector<std::size_t> worklist{0U};
        reachable[0] = true;
        while (!worklist.empty()) {
            const auto index = worklist.back();
            worklist.pop_back();
            for (const auto& instruction : function.blocks[index].instructions) {
                for (const auto& successor : instruction.successors) {
                    const auto found = block_indices.find(successor.block);
                    if (found == block_indices.end() || reachable[found->second]) continue;
                    reachable[found->second] = true;
                    worklist.push_back(found->second);
                }
            }
        }
        std::vector<Block> retained_blocks;
        retained_blocks.reserve(function.blocks.size());
        for (std::size_t index = 0; index < function.blocks.size(); ++index) {
            if (!reachable[index]) {
                ++stats.unreachable_blocks_removed;
                continue;
            }
            retained_blocks.push_back(std::move(function.blocks[index]));
        }
        function.blocks = std::move(retained_blocks);
    }

    // Trace-schedule reachable blocks so likely successor edges become physical
    // fallthroughs. This is deliberately frequency-neutral: it follows the sole
    // successor of jumps and the false successor of branches (matching conventional
    // forward-not-taken layout), then appends any
    // remaining blocks in their original order for deterministic output.
    if (function.blocks.size() > 1U) {
        std::unordered_map<std::string, std::size_t> block_indices;
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            block_indices.emplace(function.blocks[index].name, index);
        std::vector<bool> scheduled(function.blocks.size(), false);
        std::vector<std::size_t> order;
        order.reserve(function.blocks.size());
        const auto schedule_trace = [&](std::size_t start) {
            auto current = start;
            while (current < function.blocks.size() && !scheduled[current]) {
                scheduled[current] = true;
                order.push_back(current);
                const auto& instructions = function.blocks[current].instructions;
                if (instructions.empty()) break;
                const auto& terminator = instructions.back();
                if ((terminator.opcode != Opcode::jump && terminator.opcode != Opcode::branch_i1) ||
                    terminator.successors.empty()) break;
                const auto preferred_successor = terminator.opcode == Opcode::branch_i1 && terminator.successors.size() == 2U
                    ? 1U : 0U;
                const auto found = block_indices.find(terminator.successors[preferred_successor].block);
                if (found == block_indices.end() || scheduled[found->second]) break;
                current = found->second;
            }
        };
        schedule_trace(0U);
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            if (!scheduled[index]) schedule_trace(index);
        bool changed_order = false;
        for (std::size_t index = 0; index < order.size(); ++index)
            changed_order = changed_order || order[index] != index;
        if (changed_order) {
            std::vector<Block> reordered;
            reordered.reserve(function.blocks.size());
            for (const auto index : order) reordered.push_back(std::move(function.blocks[index]));
            function.blocks = std::move(reordered);
            stats.blocks_reordered = 1U;
        }
    }

    // Rotate simple two-block loops into entry, body, header, exit order.  The
    // loop body then falls through to the header, while the header emits the
    // sole conditional backedge to the body.  This removes the unconditional
    // jump from the hot path without duplicating blocks or changing CFG edges.
    // Restrict the transform to a single-predecessor body so placing it before
    // the header cannot create an accidental fallthrough from another block.
    if (function.blocks.size() > 2U) {
        std::unordered_map<std::string, std::size_t> indices;
        std::unordered_map<std::string, std::uint32_t> predecessors;
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            indices.emplace(function.blocks[index].name, index);
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
                for (const auto& successor : instruction.successors)
                    ++predecessors[successor.block];

        bool rotated = true;
        while (rotated) {
            rotated = false;
            indices.clear();
            for (std::size_t index = 0; index < function.blocks.size(); ++index)
                indices.emplace(function.blocks[index].name, index);
            for (std::size_t header_index = 1U; header_index < function.blocks.size(); ++header_index) {
                const auto& header = function.blocks[header_index];
                if (header.instructions.empty()) continue;
                const auto& branch = header.instructions.back();
                if (branch.opcode != Opcode::branch_i1 || branch.successors.size() != 2U) continue;
                for (const auto& successor : branch.successors) {
                    const auto body_found = indices.find(successor.block);
                    if (body_found == indices.end() || body_found->second == 0U) continue;
                    const auto body_index = body_found->second;
                    const auto& body = function.blocks[body_index];
                    if (body.instructions.empty() || predecessors[body.name] != 1U) continue;
                    const auto& jump = body.instructions.back();
                    if (jump.opcode != Opcode::jump || jump.successors.size() != 1U ||
                        jump.successors.front().block != header.name) continue;
                    if (body_index + 1U == header_index) continue;

                    auto moved = std::move(function.blocks[body_index]);
                    function.blocks.erase(function.blocks.begin() + static_cast<std::ptrdiff_t>(body_index));
                    auto adjusted_header = header_index;
                    if (body_index < header_index) --adjusted_header;
                    function.blocks.insert(function.blocks.begin() + static_cast<std::ptrdiff_t>(adjusted_header),
                                           std::move(moved));
                    stats.blocks_reordered = 1U;
                    rotated = true;
                    break;
                }
                if (rotated) break;
            }
        }
    }

    std::vector<VirtualRegister> aliases(function.register_count);
    std::iota(aliases.begin(), aliases.end(), VirtualRegister{0});
    std::vector<bool> folded_address(function.register_count, false);
    std::vector<bool> eliminated_extension(function.register_count, false);

    // Fold a single-use pointer offset into the displacement field of the
    // consuming load/store. x86-64 can encode base+disp32 directly, avoiding
    // a temporary virtual register and a separate add instruction.
    std::vector<std::uint32_t> use_counts(function.register_count, 0U);
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            for (const auto input : instruction.inputs) if (input < use_counts.size()) ++use_counts[input];
            for (const auto& successor : instruction.successors)
                for (const auto argument : successor.arguments) if (argument < use_counts.size()) ++use_counts[argument];
        }
    }

    // Fold a single-use integer constant into arithmetic and shift instructions.
    // This removes the constant-producing virtual register before allocation and
    // lets x86-64 select its native immediate forms directly.
    std::vector<Instruction*> definitions(function.register_count, nullptr);
    for (auto& block : function.blocks)
        for (auto& instruction : block.instructions)
            if (has_result(instruction.opcode) && instruction.result < definitions.size()) definitions[instruction.result] = &instruction;
    std::vector<bool> eliminated_constant(function.register_count, false);
    std::vector<std::uint32_t> folded_constant_uses(function.register_count, 0U);
    std::vector<std::uint32_t> immediate_capable_uses(function.register_count, 0U);

    // Strength-reduce unsigned division and remainder by a power-of-two constant.
    // This is valid for every unsigned input and replaces the expensive DIV
    // instruction with a shift or mask. Keep the constant definition until all
    // of its uses have been folded so mixed-use constants remain correct.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            const bool unsigned_division = instruction.opcode == Opcode::div_u_i32 ||
                                           instruction.opcode == Opcode::div_u_i64;
            const bool unsigned_remainder = instruction.opcode == Opcode::rem_u_i32 ||
                                            instruction.opcode == Opcode::rem_u_i64;
            if ((!unsigned_division && !unsigned_remainder) || instruction.inputs.size() != 2U) continue;
            const auto divisor_register = instruction.inputs[1];
            const auto* divisor_definition = divisor_register < definitions.size()
                ? definitions[divisor_register] : nullptr;
            if (divisor_definition == nullptr ||
                (divisor_definition->opcode != Opcode::load_immediate &&
                 divisor_definition->opcode != Opcode::load_immediate_i64)) continue;
            const auto divisor = static_cast<std::uint64_t>(divisor_definition->immediate);
            if (divisor == 0U || (divisor & (divisor - 1U)) != 0U) continue;

            const bool wide = instruction.opcode == Opcode::div_u_i64 ||
                              instruction.opcode == Opcode::rem_u_i64;
            instruction.inputs.resize(1U);
            instruction.symbol = "$imm";
            if (unsigned_division) {
                std::uint32_t shift = 0U;
                auto value = divisor;
                while (value > 1U) { value >>= 1U; ++shift; }
                instruction.opcode = wide ? Opcode::shr_u_i64 : Opcode::shr_u_i32;
                instruction.immediate = static_cast<std::int64_t>(shift);
            } else {
                instruction.opcode = wide ? Opcode::and_i64 : Opcode::and_i32;
                instruction.immediate = static_cast<std::int64_t>(divisor - 1U);
            }
            ++folded_constant_uses[divisor_register];
            ++stats.immediate_forms_selected;
        }
    }
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (supports_integer_immediate(instruction.opcode) && instruction.inputs.size() == 2U) {
                const auto right = instruction.inputs[1];
                if (right < definitions.size() && definitions[right] != nullptr &&
                    (definitions[right]->opcode == Opcode::load_immediate ||
                     definitions[right]->opcode == Opcode::load_immediate_i64))
                    ++immediate_capable_uses[right];
                if (is_commutative_integer(instruction.opcode)) {
                    const auto left = instruction.inputs[0];
                    if (left < definitions.size() && definitions[left] != nullptr &&
                        (definitions[left]->opcode == Opcode::load_immediate ||
                         definitions[left]->opcode == Opcode::load_immediate_i64))
                        ++immediate_capable_uses[left];
                }
            } else if (is_integer_comparison(instruction.opcode) && instruction.inputs.size() == 2U) {
                const auto right = instruction.inputs[1];
                if (right < definitions.size() && definitions[right] != nullptr &&
                    (definitions[right]->opcode == Opcode::load_immediate ||
                     definitions[right]->opcode == Opcode::load_immediate_i64))
                    ++immediate_capable_uses[right];
            }
        }
    }
    std::vector<bool> eliminated_load(function.register_count, false);
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            if (!supports_integer_immediate(instruction.opcode) || instruction.inputs.size() != 2U) continue;
            std::size_t constant_index = 1U;
            auto reg = instruction.inputs[constant_index];
            auto* definition = reg < definitions.size() ? definitions[reg] : nullptr;
            if ((definition == nullptr ||
                 (definition->opcode != Opcode::load_immediate && definition->opcode != Opcode::load_immediate_i64)) &&
                is_commutative_integer(instruction.opcode)) {
                constant_index = 0U;
                reg = instruction.inputs[constant_index];
                definition = reg < definitions.size() ? definitions[reg] : nullptr;
            }
            if (definition == nullptr ||
                (definition->opcode != Opcode::load_immediate && definition->opcode != Opcode::load_immediate_i64)) continue;
            const auto value = definition->immediate;
            const bool wide = instruction.opcode == Opcode::add_i64 || instruction.opcode == Opcode::sub_i64 ||
                              instruction.opcode == Opcode::mul_i64 || instruction.opcode == Opcode::and_i64 ||
                              instruction.opcode == Opcode::or_i64 || instruction.opcode == Opcode::xor_i64 ||
                              instruction.opcode == Opcode::shl_i64 || instruction.opcode == Opcode::shr_s_i64 ||
                              instruction.opcode == Opcode::shr_u_i64;
            if (is_shift(instruction.opcode)) {
                if (value < 0 || value > 255) continue;
            } else if (wide && (value < std::numeric_limits<std::int32_t>::min() ||
                                value > std::numeric_limits<std::int32_t>::max())) {
                continue;
            }
            if (constant_index == 0U) std::swap(instruction.inputs[0], instruction.inputs[1]);
            instruction.inputs.resize(1U);
            instruction.immediate = value;
            instruction.symbol = "$imm";
            ++folded_constant_uses[reg];
            ++stats.immediate_forms_selected;
        }
    }

    // Fold a single-use right-hand integer constant into comparisons. The
    // comparison remains an SSA definition until branch fusion below, but no
    // virtual register or allocation is needed for the constant operand.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            if (!is_integer_comparison(instruction.opcode) || instruction.inputs.size() != 2U) continue;
            const auto constant = instruction.inputs[1];
            auto* definition = constant < definitions.size() ? definitions[constant] : nullptr;
            if (definition == nullptr ||
                (definition->opcode != Opcode::load_immediate && definition->opcode != Opcode::load_immediate_i64)) continue;
            const auto value = definition->immediate;
            const bool wide = instruction.opcode >= Opcode::cmp_eq_i64;
            if (wide && (value < std::numeric_limits<std::int32_t>::min() ||
                         value > std::numeric_limits<std::int32_t>::max())) continue;
            instruction.inputs.resize(1U);
            instruction.immediate = value;
            instruction.symbol = "$cmpimm";
            ++folded_constant_uses[constant];
            ++stats.immediate_comparisons_selected;
        }
    }

    // A constant may be folded into only some of its uses. Keep its materialization
    // for edge arguments, ABI uses, or unsupported operations, and remove it only
    // when every original use has been rewritten. This shortens arithmetic live
    // ranges without corrupting mixed-use constants such as loop initializers.
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
        if (use_counts[reg] != 0U && folded_constant_uses[reg] == use_counts[reg]) {
            eliminated_constant[reg] = true;
            ++stats.constant_definitions_eliminated;
        }
    }

    // Fold single-use integer constants directly into stack and pointer stores.
    // x86-64 supports immediate-to-memory encodings for i8/i16/i32 and for i64
    // values representable as a sign-extended imm32. The pointer operand remains
    // live for pointer stores; only the constant value register disappears.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            const bool stack_store = instruction.opcode == Opcode::store_stack_i8 ||
                                     instruction.opcode == Opcode::store_stack_i16 ||
                                     instruction.opcode == Opcode::store_stack_i32 ||
                                     instruction.opcode == Opcode::store_stack_i64;
            const bool pointer_store = instruction.opcode == Opcode::store_ptr_i8 ||
                                       instruction.opcode == Opcode::store_ptr_i16 ||
                                       instruction.opcode == Opcode::store_ptr_i32 ||
                                       instruction.opcode == Opcode::store_ptr_i64;
            if ((!stack_store && !pointer_store) || instruction.inputs.empty()) continue;
            const auto value_reg = instruction.inputs.front();
            auto* definition = value_reg < definitions.size() ? definitions[value_reg] : nullptr;
            if (definition == nullptr || use_counts[value_reg] != 1U ||
                (definition->opcode != Opcode::load_immediate && definition->opcode != Opcode::load_immediate_i64)) continue;
            const auto value = definition->immediate;
            const bool wide = instruction.opcode == Opcode::store_stack_i64 || instruction.opcode == Opcode::store_ptr_i64;
            if (wide && (value < std::numeric_limits<std::int32_t>::min() ||
                         value > std::numeric_limits<std::int32_t>::max())) continue;
            instruction.inputs.erase(instruction.inputs.begin());
            instruction.argument_index = static_cast<std::uint32_t>(value);
            instruction.symbol = "$storeimm";
            eliminated_constant[value_reg] = true;
            ++stats.constant_stores_selected;
            ++stats.constant_definitions_eliminated;
        }
    }

    // Materialize single-use integer constants directly in the ABI return
    // register. Zero uses xor eax,eax, avoiding both a virtual register and a
    // longer mov-immediate encoding.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            if ((instruction.opcode != Opcode::return_i32 && instruction.opcode != Opcode::return_i64) ||
                instruction.inputs.size() != 1U) continue;
            const auto value_reg = instruction.inputs.front();
            auto* definition = value_reg < definitions.size() ? definitions[value_reg] : nullptr;
            if (definition == nullptr || use_counts[value_reg] != 1U ||
                (definition->opcode != Opcode::load_immediate && definition->opcode != Opcode::load_immediate_i64)) continue;
            instruction.inputs.clear();
            instruction.immediate = definition->immediate;
            instruction.symbol = "$retimm";
            eliminated_constant[value_reg] = true;
            ++stats.direct_constant_returns;
            ++stats.constant_definitions_eliminated;
            if (instruction.immediate == 0) ++stats.zeroing_idioms_selected;
        }
    }

    // Fold a single-use local stack load directly into an integer return. The
    // ABI return register is the natural destination of the memory load, so the
    // temporary virtual register and its allocation are unnecessary. Narrow
    // i8/i16 loads retain their zero-extension semantics.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            if ((instruction.opcode != Opcode::return_i32 && instruction.opcode != Opcode::return_i64) ||
                instruction.inputs.size() != 1U) continue;
            const auto value_reg = instruction.inputs.front();
            auto* definition = value_reg < definitions.size() ? definitions[value_reg] : nullptr;
            if (definition == nullptr || use_counts[value_reg] != 1U) continue;
            std::uint32_t width = 0U;
            switch (definition->opcode) {
            case Opcode::load_stack_i8: width = 1U; break;
            case Opcode::load_stack_i16: width = 2U; break;
            case Opcode::load_stack_i32: width = 4U; break;
            case Opcode::load_stack_i64: width = 8U; break;
            default: continue;
            }
            if ((instruction.opcode == Opcode::return_i64 && width != 8U) ||
                (instruction.opcode == Opcode::return_i32 && width == 8U)) continue;
            instruction.inputs.clear();
            instruction.immediate = definition->immediate;
            instruction.argument_index = width;
            instruction.symbol = "$retloadstack";
            eliminated_load[value_reg] = true;
            ++stats.load_returns_folded;
        }
    }

    // Fold a single-use local-stack load into the right-hand memory operand of
    // integer arithmetic. x86-64 can consume [rbp+disp32] directly, removing
    // the temporary load result before liveness and register allocation.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            const bool supported = instruction.opcode == Opcode::add_i32 || instruction.opcode == Opcode::add_i64 ||
                                   instruction.opcode == Opcode::sub_i32 || instruction.opcode == Opcode::sub_i64 ||
                                   instruction.opcode == Opcode::mul_i32 || instruction.opcode == Opcode::mul_i64 ||
                                   instruction.opcode == Opcode::and_i32 || instruction.opcode == Opcode::and_i64 ||
                                   instruction.opcode == Opcode::or_i32 || instruction.opcode == Opcode::or_i64 ||
                                   instruction.opcode == Opcode::xor_i32 || instruction.opcode == Opcode::xor_i64;
            if (!supported || instruction.inputs.size() != 2U || !instruction.symbol.empty()) continue;
            const auto loaded_reg = instruction.inputs[1];
            auto* definition = loaded_reg < definitions.size() ? definitions[loaded_reg] : nullptr;
            if (definition == nullptr || use_counts[loaded_reg] != 1U) continue;
            const bool wide = instruction.opcode == Opcode::add_i64 || instruction.opcode == Opcode::sub_i64 ||
                              instruction.opcode == Opcode::mul_i64 || instruction.opcode == Opcode::and_i64 ||
                              instruction.opcode == Opcode::or_i64 || instruction.opcode == Opcode::xor_i64;
            if ((wide && definition->opcode != Opcode::load_stack_i64) ||
                (!wide && definition->opcode != Opcode::load_stack_i32)) continue;
            instruction.inputs.resize(1U);
            instruction.immediate = definition->immediate;
            instruction.symbol = "$memstack";
            eliminated_load[loaded_reg] = true;
            ++stats.load_arithmetic_folded;
        }
    }

    // Fold single-use pointer loads into integer arithmetic memory operands.
    // This is the pointer analogue of the local-stack fold above and maps to
    // x86-64 reg,[base+disp] forms. If the load address is itself a single-use
    // ptr.offset, absorb that displacement too so both temporary definitions
    // disappear before register allocation.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            const bool supported = instruction.opcode == Opcode::add_i32 || instruction.opcode == Opcode::add_i64 ||
                                   instruction.opcode == Opcode::sub_i32 || instruction.opcode == Opcode::sub_i64 ||
                                   instruction.opcode == Opcode::mul_i32 || instruction.opcode == Opcode::mul_i64 ||
                                   instruction.opcode == Opcode::and_i32 || instruction.opcode == Opcode::and_i64 ||
                                   instruction.opcode == Opcode::or_i32 || instruction.opcode == Opcode::or_i64 ||
                                   instruction.opcode == Opcode::xor_i32 || instruction.opcode == Opcode::xor_i64;
            if (!supported || instruction.inputs.size() != 2U || !instruction.symbol.empty()) continue;
            const bool wide = instruction.opcode == Opcode::add_i64 || instruction.opcode == Opcode::sub_i64 ||
                              instruction.opcode == Opcode::mul_i64 || instruction.opcode == Opcode::and_i64 ||
                              instruction.opcode == Opcode::or_i64 || instruction.opcode == Opcode::xor_i64;
            const bool commutative = instruction.opcode != Opcode::sub_i32 && instruction.opcode != Opcode::sub_i64;

            std::size_t loaded_index = 1U;
            auto loaded_reg = instruction.inputs[loaded_index];
            auto* load = loaded_reg < definitions.size() ? definitions[loaded_reg] : nullptr;
            const auto matches_load = [&](const Instruction* candidate) {
                return candidate != nullptr && candidate->inputs.size() == 1U &&
                       ((wide && candidate->opcode == Opcode::load_ptr_i64) ||
                        (!wide && candidate->opcode == Opcode::load_ptr_i32));
            };
            if ((!matches_load(load) || use_counts[loaded_reg] != 1U) && commutative) {
                loaded_index = 0U;
                loaded_reg = instruction.inputs[loaded_index];
                load = loaded_reg < definitions.size() ? definitions[loaded_reg] : nullptr;
            }
            if (!matches_load(load) || use_counts[loaded_reg] != 1U) continue;
            if (loaded_index == 0U) std::swap(instruction.inputs[0], instruction.inputs[1]);

            auto pointer = load->inputs.front();
            auto displacement = load->immediate;
            auto* pointer_definition = pointer < definitions.size() ? definitions[pointer] : nullptr;
            if (pointer_definition != nullptr && pointer_definition->opcode == Opcode::ptr_offset &&
                pointer_definition->inputs.size() == 1U && use_counts[pointer] == 1U) {
                const auto combined = displacement + pointer_definition->immediate;
                if (combined >= std::numeric_limits<std::int32_t>::min() &&
                    combined <= std::numeric_limits<std::int32_t>::max()) {
                    displacement = combined;
                    pointer = pointer_definition->inputs.front();
                    pointer_definition->immediate = 0;
                    folded_address[load->inputs.front()] = true;
                }
            }
            if (displacement < std::numeric_limits<std::int32_t>::min() ||
                displacement > std::numeric_limits<std::int32_t>::max()) continue;

            instruction.inputs[1] = pointer;
            instruction.immediate = displacement;
            instruction.symbol = "$memptr";
            eliminated_load[loaded_reg] = true;
            ++stats.load_arithmetic_folded;
        }
    }

    // Fold single-use floating pointer loads into scalar SSE arithmetic
    // memory operands. This keeps array/struct kernels from allocating an XMM
    // register (or spill slot) solely to hold a value consumed by one add/mul.
    for (auto& block : function.blocks) {
        for (auto& instruction : block.instructions) {
            const bool supported = instruction.opcode == Opcode::add_f32 || instruction.opcode == Opcode::add_f64 ||
                                   instruction.opcode == Opcode::sub_f32 || instruction.opcode == Opcode::sub_f64 ||
                                   instruction.opcode == Opcode::mul_f32 || instruction.opcode == Opcode::mul_f64 ||
                                   instruction.opcode == Opcode::div_f32 || instruction.opcode == Opcode::div_f64;
            if (!supported || instruction.inputs.size() != 2U || !instruction.symbol.empty()) continue;
            const bool wide = instruction.opcode == Opcode::add_f64 || instruction.opcode == Opcode::sub_f64 ||
                              instruction.opcode == Opcode::mul_f64 || instruction.opcode == Opcode::div_f64;
            const bool commutative = instruction.opcode == Opcode::add_f32 || instruction.opcode == Opcode::add_f64 ||
                                     instruction.opcode == Opcode::mul_f32 || instruction.opcode == Opcode::mul_f64;
            std::size_t loaded_index = 1U;
            auto loaded_reg = instruction.inputs[loaded_index];
            auto* load = loaded_reg < definitions.size() ? definitions[loaded_reg] : nullptr;
            const auto matches = [&](const Instruction* candidate) {
                return candidate != nullptr && candidate->inputs.size() == 1U &&
                       ((wide && candidate->opcode == Opcode::load_ptr_f64) ||
                        (!wide && candidate->opcode == Opcode::load_ptr_f32));
            };
            if ((!matches(load) || use_counts[loaded_reg] != 1U) && commutative) {
                loaded_index = 0U;
                loaded_reg = instruction.inputs[loaded_index];
                load = loaded_reg < definitions.size() ? definitions[loaded_reg] : nullptr;
            }
            if (!matches(load) || use_counts[loaded_reg] != 1U) continue;
            if (loaded_index == 0U) std::swap(instruction.inputs[0], instruction.inputs[1]);

            auto pointer = load->inputs.front();
            auto displacement = load->immediate;
            auto* pointer_definition = pointer < definitions.size() ? definitions[pointer] : nullptr;
            if (pointer_definition != nullptr && pointer_definition->opcode == Opcode::ptr_offset &&
                pointer_definition->inputs.size() == 1U && use_counts[pointer] == 1U) {
                const auto combined = displacement + pointer_definition->immediate;
                if (combined >= std::numeric_limits<std::int32_t>::min() &&
                    combined <= std::numeric_limits<std::int32_t>::max()) {
                    displacement = combined;
                    pointer = pointer_definition->inputs.front();
                    pointer_definition->immediate = 0;
                    folded_address[load->inputs.front()] = true;
                }
            }
            if (displacement < std::numeric_limits<std::int32_t>::min() ||
                displacement > std::numeric_limits<std::int32_t>::max()) continue;
            instruction.inputs[1] = pointer;
            instruction.immediate = displacement;
            instruction.symbol = "$memptr";
            eliminated_load[loaded_reg] = true;
            ++stats.load_arithmetic_folded;
        }
    }

    // Fuse a comparison used only by a conditional branch. The branch retains
    // the original comparison operands and condition in its immediate field,
    // allowing the encoder to emit cmp+jcc directly without materializing i1.
    std::vector<bool> fused_compare(function.register_count, false);
    std::vector<bool> eliminated_test_mask(function.register_count, false);
    for (auto& block : function.blocks) {
        for (auto& select : block.instructions) {
            if ((select.opcode != Opcode::select_i32 && select.opcode != Opcode::select_i64) ||
                select.inputs.size() != 3U) continue;
            const auto condition = select.inputs[0];
            if (condition >= function.register_count || use_counts[condition] != 1U) continue;
            auto* comparison = condition < definitions.size() ? definitions[condition] : nullptr;
            if (comparison == nullptr || comparison->symbol != "$cmpimm" || comparison->immediate != 0 ||
                (comparison->opcode != Opcode::cmp_eq_i32 && comparison->opcode != Opcode::cmp_ne_i32 &&
                 comparison->opcode != Opcode::cmp_eq_i64 && comparison->opcode != Opcode::cmp_ne_i64) ||
                comparison->inputs.size() != 1U) continue;
            const auto masked = comparison->inputs[0];
            auto* producer = masked < definitions.size() ? definitions[masked] : nullptr;
            if (producer == nullptr || (producer->opcode != Opcode::and_i32 && producer->opcode != Opcode::and_i64) ||
                producer->symbol != "$imm" || producer->inputs.size() != 1U || use_counts[masked] != 1U ||
                producer->immediate < std::numeric_limits<std::int32_t>::min() ||
                producer->immediate > std::numeric_limits<std::int32_t>::max()) continue;
            select.inputs[0] = producer->inputs[0];
            select.symbol = "$testimm";
            select.argument_index = static_cast<std::uint32_t>(comparison->opcode) + 1U;
            select.immediate = producer->immediate;
            fused_compare[condition] = true;
            eliminated_test_mask[masked] = true;
        }
    }

    for (auto& block : function.blocks) {
        for (auto& branch : block.instructions) {
            if (branch.opcode != Opcode::branch_i1 || branch.inputs.size() != 1U) continue;
            const auto condition = branch.inputs.front();
            if (condition >= function.register_count || use_counts[condition] != 1U) continue;
            Instruction* comparison = nullptr;
            for (auto& search_block : function.blocks) {
                for (auto& candidate : search_block.instructions) {
                    if (candidate.result == condition && is_integer_comparison(candidate.opcode) &&
                        (candidate.inputs.size() == 2U ||
                         (candidate.inputs.size() == 1U && candidate.symbol == "$cmpimm"))) {
                        comparison = &candidate;
                        break;
                    }
                }
                if (comparison != nullptr) break;
            }
            if (comparison == nullptr) continue;
            branch.inputs = comparison->inputs;
            bool reuse_arithmetic_flags = false;
            bool use_test_immediate = false;
            if (comparison->symbol == "$cmpimm" && comparison->immediate == 0 &&
                (comparison->opcode == Opcode::cmp_eq_i32 || comparison->opcode == Opcode::cmp_ne_i32 ||
                 comparison->opcode == Opcode::cmp_eq_i64 || comparison->opcode == Opcode::cmp_ne_i64) &&
                !comparison->inputs.empty()) {
                const auto masked = comparison->inputs.front();
                auto* producer = masked < definitions.size() ? definitions[masked] : nullptr;
                if (producer != nullptr &&
                    (producer->opcode == Opcode::and_i32 || producer->opcode == Opcode::and_i64) &&
                    producer->symbol == "$imm" && producer->inputs.size() == 1U &&
                    masked < use_counts.size() && use_counts[masked] == 1U &&
                    producer->immediate >= std::numeric_limits<std::int32_t>::min() &&
                    producer->immediate <= std::numeric_limits<std::int32_t>::max()) {
                    branch.inputs = producer->inputs;
                    branch.symbol = "$testimm";
                    branch.argument_index = static_cast<std::uint32_t>(comparison->opcode) + 1U;
                    branch.immediate = producer->immediate;
                    eliminated_test_mask[masked] = true;
                    use_test_immediate = true;
                }
            }
            if (comparison->symbol == "$cmpimm" && comparison->immediate == 0 &&
                (comparison->opcode == Opcode::cmp_eq_i32 || comparison->opcode == Opcode::cmp_ne_i32 ||
                 comparison->opcode == Opcode::cmp_eq_i64 || comparison->opcode == Opcode::cmp_ne_i64)) {
                const auto comparison_index = static_cast<std::size_t>(comparison - block.instructions.data());
                if (comparison_index > 0U && &block.instructions[comparison_index + 1U] == &branch) {
                    const auto& producer = block.instructions[comparison_index - 1U];
                    const bool flag_setting_integer = producer.opcode == Opcode::add_i32 || producer.opcode == Opcode::add_i64 ||
                        producer.opcode == Opcode::sub_i32 || producer.opcode == Opcode::sub_i64 ||
                        producer.opcode == Opcode::and_i32 || producer.opcode == Opcode::and_i64 ||
                        producer.opcode == Opcode::or_i32 || producer.opcode == Opcode::or_i64 ||
                        producer.opcode == Opcode::xor_i32 || producer.opcode == Opcode::xor_i64;
                    reuse_arithmetic_flags = flag_setting_integer && !comparison->inputs.empty() &&
                        producer.result == comparison->inputs.front();
                }
            }
            if (use_test_immediate) {
                // Already encoded above as a direct TEST of the original value.
            } else if (reuse_arithmetic_flags) {
                branch.symbol = "$flags";
                branch.argument_index = static_cast<std::uint32_t>(comparison->opcode) + 1U;
                branch.immediate = 0;
            } else if (comparison->symbol == "$cmpimm") {
                branch.symbol = "$cmpimm";
                branch.argument_index = static_cast<std::uint32_t>(comparison->opcode) + 1U;
                branch.immediate = comparison->immediate;
            } else {
                branch.immediate = static_cast<std::int64_t>(comparison->opcode) + 1;
            }
            fused_compare[condition] = true;
            ++stats.compare_branches_fused;
            stats.compare_branch_bytes_avoided += 8U;
        }
    }

    // Fuse ordered floating relational comparisons into branches.  Equality
    // and inequality need an explicit PF (unordered/NaN) combination, so they
    // remain materialized until the branch representation can express two
    // flag predicates.  Relational comparisons can be made NaN-correct with
    // a single unsigned condition by selecting the operand order carefully.
    for (auto& block : function.blocks) {
        for (auto& branch : block.instructions) {
            if (branch.opcode != Opcode::branch_i1 || branch.inputs.size() != 1U) continue;
            const auto condition = branch.inputs.front();
            if (condition >= function.register_count || use_counts[condition] != 1U) continue;
            Instruction* comparison = nullptr;
            for (auto& search_block : function.blocks) {
                for (auto& candidate : search_block.instructions) {
                    if (candidate.result == condition && is_floating_relational_comparison(candidate.opcode) &&
                        candidate.inputs.size() == 2U) {
                        comparison = &candidate;
                        break;
                    }
                }
                if (comparison != nullptr) break;
            }
            if (comparison == nullptr) continue;
            branch.inputs = comparison->inputs;
            branch.symbol = "$fcmp";
            branch.argument_index = static_cast<std::uint32_t>(comparison->opcode) + 1U;
            branch.immediate = 0;
            fused_compare[condition] = true;
            ++stats.floating_compare_branches_fused;
            stats.floating_compare_branch_bytes_avoided += 8U;
        }
    }
    // Fold ptr.offset values when every effective live use is an address use.
    // A single address temporary is commonly shared by a load and store; x86
    // can encode [base+disp] independently at both sites, so requiring exactly
    // one SSA use needlessly materializes address registers. Loads previously
    // folded into $memptr arithmetic remain in the machine list until cleanup;
    // ignore those dead definitions and reason about the replacement address
    // operand instead.
    for (auto& block : function.blocks) {
        for (auto& definition : block.instructions) {
            if (definition.opcode != Opcode::ptr_offset || definition.inputs.size() != 1U ||
                definition.result >= function.register_count || definition.immediate == 0) continue;

            struct AddressUse { Instruction* instruction; std::size_t operand_index; };
            std::vector<AddressUse> address_uses;
            bool unsafe_use = false;
            for (auto& search_block : function.blocks) {
                for (auto& candidate : search_block.instructions) {
                    if (candidate.result < eliminated_load.size() && eliminated_load[candidate.result])
                        continue;
                    bool matched = false;
                    if (is_pointer_memory_operation(candidate.opcode)) {
                        const auto index = pointer_operand_index(candidate.opcode);
                        if (candidate.inputs.size() > index && candidate.inputs[index] == definition.result) {
                            address_uses.push_back({&candidate, index});
                            matched = true;
                        }
                    }
                    if (!matched && candidate.symbol == "$memptr" && candidate.inputs.size() >= 2U &&
                        candidate.inputs[1] == definition.result) {
                        address_uses.push_back({&candidate, 1U});
                        matched = true;
                    }
                    if (matched) continue;
                    if (std::find(candidate.inputs.begin(), candidate.inputs.end(), definition.result) != candidate.inputs.end()) {
                        unsafe_use = true;
                        break;
                    }
                    for (const auto& successor : candidate.successors) {
                        if (std::find(successor.arguments.begin(), successor.arguments.end(), definition.result) != successor.arguments.end()) {
                            unsafe_use = true;
                            break;
                        }
                    }
                    if (unsafe_use) break;
                }
                if (unsafe_use) break;
            }
            if (unsafe_use || address_uses.empty()) continue;

            bool encodable = true;
            for (const auto& use : address_uses) {
                const auto displacement = definition.immediate + use.instruction->immediate;
                if (displacement < std::numeric_limits<std::int32_t>::min() ||
                    displacement > std::numeric_limits<std::int32_t>::max()) {
                    encodable = false;
                    break;
                }
            }
            if (!encodable) continue;

            for (const auto& use : address_uses) {
                use.instruction->inputs[use.operand_index] = definition.inputs.front();
                use.instruction->immediate += definition.immediate;
            }
            definition.immediate = 0;
            folded_address[definition.result] = true;
            stats.address_modes_folded += static_cast<std::uint32_t>(address_uses.size());
        }
    }

    // Eliminate inverse extension chains such as truncate(zero_extend(x)) and
    // truncate(sign_extend(x)) when the truncate restores the original width.
    // The high bits introduced by the extension are discarded, so the chain is
    // exactly equivalent to the original SSA value.
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (instruction.opcode != Opcode::truncate || instruction.inputs.size() != 1U ||
                instruction.result >= function.register_count) continue;
            const auto extended = instruction.inputs.front();
            const auto* definition = extended < definitions.size() ? definitions[extended] : nullptr;
            if (definition == nullptr || definition->inputs.size() != 1U ||
                (definition->opcode != Opcode::zero_extend && definition->opcode != Opcode::sign_extend)) continue;
            const auto original_bits = static_cast<unsigned>((definition->immediate >> 8U) & 0xffU);
            const auto extended_bits = static_cast<unsigned>(definition->immediate & 0xffU);
            const auto truncate_source_bits = static_cast<unsigned>((instruction.immediate >> 8U) & 0xffU);
            const auto truncate_result_bits = static_cast<unsigned>(instruction.immediate & 0xffU);
            if (original_bits == 0U || extended_bits == 0U || truncate_source_bits != extended_bits ||
                truncate_result_bits != original_bits) continue;
            const auto original = definition->inputs.front();
            if (!compatible(function, instruction.result, original)) continue;
            aliases[instruction.result] = original;
            if (extended < use_counts.size() && use_counts[extended] == 1U) eliminated_extension[extended] = true;
        }
    }

    // Machine IR is SSA. Alias-producing operations can therefore be collected
    // independently of block layout and applied transitively to every use.
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (instruction.inputs.size() != 1U || instruction.result >= function.register_count) continue;
            const auto source = instruction.inputs.front();
            if (!compatible(function, instruction.result, source)) continue;
            if (is_copy(instruction.opcode) ||
                (instruction.opcode == Opcode::ptr_offset && instruction.immediate == 0) ||
                is_redundant_cast(instruction)) {
                aliases[instruction.result] = source;
            }
        }
    }
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg)
        aliases[reg] = resolve_alias(aliases, reg);

    for (auto& block : function.blocks) {
        std::vector<Instruction> optimized;
        optimized.reserve(block.instructions.size());
        for (auto instruction : block.instructions) {
            for (auto& input : instruction.inputs) input = resolve_alias(aliases, input);
            for (auto& successor : instruction.successors)
                for (auto& argument : successor.arguments) argument = resolve_alias(aliases, argument);

            if (instruction.result < fused_compare.size() && fused_compare[instruction.result] &&
                is_integer_comparison(instruction.opcode)) {
                continue;
            }
            if (instruction.result < eliminated_test_mask.size() && eliminated_test_mask[instruction.result] &&
                (instruction.opcode == Opcode::and_i32 || instruction.opcode == Opcode::and_i64)) {
                continue;
            }
            if (instruction.result < eliminated_constant.size() && eliminated_constant[instruction.result] &&
                (instruction.opcode == Opcode::load_immediate || instruction.opcode == Opcode::load_immediate_i64)) {
                continue;
            }
            if (instruction.result < eliminated_load.size() && eliminated_load[instruction.result] &&
                (instruction.opcode == Opcode::load_stack_i8 || instruction.opcode == Opcode::load_stack_i16 ||
                 instruction.opcode == Opcode::load_stack_i32 || instruction.opcode == Opcode::load_stack_i64 ||
                 instruction.opcode == Opcode::load_ptr_i32 || instruction.opcode == Opcode::load_ptr_i64)) {
                continue;
            }
            if (instruction.result < eliminated_extension.size() && eliminated_extension[instruction.result] &&
                (instruction.opcode == Opcode::zero_extend || instruction.opcode == Opcode::sign_extend)) {
                ++stats.extension_chains_eliminated;
                continue;
            }

            const bool alias_definition = instruction.inputs.size() == 1U &&
                instruction.result < function.register_count &&
                aliases[instruction.result] != instruction.result;
            if (alias_definition) {
                if (is_copy(instruction.opcode)) ++stats.copies_propagated;
                else if (instruction.opcode == Opcode::ptr_offset && instruction.immediate == 0 &&
                         instruction.result < folded_address.size() && folded_address[instruction.result])
                    ++stats.address_modes_folded;
                else if (instruction.opcode == Opcode::ptr_offset && instruction.immediate == 0)
                    ++stats.zero_offsets_eliminated;
                else if (is_redundant_cast(instruction)) ++stats.redundant_casts_eliminated;
                else if (instruction.opcode == Opcode::truncate) ++stats.extension_chains_eliminated;
                else optimized.push_back(std::move(instruction));
                continue;
            }
            optimized.push_back(std::move(instruction));
        }
        block.instructions = std::move(optimized);
    }


    // Recognize straight-line in-place integer expression packs over
    // contiguous lanes. The scalar operation must be one of the SSE2-safe
    // lane-wise operations whose scalar source is shared by every lane:
    // add/sub/and/or/xor for i32 or i64. Each scalar result must feed only its
    // matching store, and the arithmetic/store groups must be adjacent so no
    // intervening memory side effect can change aliasing semantics.
    //
    // This generalizes the original i64 map-add recognizer into a small SLP
    // packer while deliberately avoiding multiplication, shifts, or floating
    // point where target support or reassociation semantics are different.
    {
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        struct ScalarLoadInfo { bool valid{}; VirtualRegister base{}; std::int64_t offset{}; Opcode opcode{}; };
        std::vector<ScalarLoadInfo> scalar_loads(function.register_count);
        for (const auto& block : function.blocks) {
            for (const auto& ins : block.instructions) {
                for (const auto input : ins.inputs) if (input < uses.size()) ++uses[input];
                if ((ins.opcode == Opcode::load_ptr_i32 || ins.opcode == Opcode::load_ptr_i64) &&
                    ins.result < scalar_loads.size() && ins.inputs.size() == 1U)
                    scalar_loads[ins.result] = {true, ins.inputs[0], ins.immediate, ins.opcode};
            }
        }
        std::unordered_set<VirtualRegister> packed_scalar_loads;

        const auto lane_width = [](Opcode opcode) -> std::size_t {
            switch (opcode) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32: return 4U;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64: return 8U;
            default: return 0U;
            }
        };
        const auto matching_store = [](Opcode arithmetic) -> Opcode {
            switch (arithmetic) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32:
                return Opcode::store_ptr_i32;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64:
                return Opcode::store_ptr_i64;
            default: return Opcode::jump; // impossible sentinel for unsupported arithmetic
            }
        };

        for (auto& block : function.blocks) {
            auto& ins = block.instructions;
            for (std::size_t start = 0; start < ins.size(); ++start) {
                bool matched = false;
                for (const std::size_t lanes : {16U, 8U, 4U, 2U}) {
                    if (start + lanes * 2U > ins.size()) continue;
                    const auto arithmetic_opcode = ins[start].opcode;
                    const auto bytes = lane_width(arithmetic_opcode);
                    if (bytes == 0U) continue;
                    const auto store_opcode = matching_store(arithmetic_opcode);
                    VirtualRegister base = function.register_count;
                    VirtualRegister store_base = function.register_count;
                    VirtualRegister scalar = function.register_count;
                    bool legal = true;
                    std::vector<VirtualRegister> candidate_scalar_loads;
                    for (std::size_t lane = 0; lane < lanes; ++lane) {
                        const auto& arithmetic = ins[start + lane];
                        const auto& store = ins[start + lanes + lane];
                        if (arithmetic.opcode != arithmetic_opcode || arithmetic.inputs.size() != 2U ||
                            store.opcode != store_opcode || store.inputs.size() != 2U ||
                            store.inputs[0] != arithmetic.result || arithmetic.result >= uses.size() ||
                            uses[arithmetic.result] != 1U ||
                            store.immediate != static_cast<std::int64_t>(lane * bytes)) {
                            legal = false; break;
                        }
                        VirtualRegister lane_scalar = function.register_count;
                        VirtualRegister lane_base = function.register_count;
                        std::int64_t lane_offset = 0;
                        VirtualRegister scalar_load_result = function.register_count;
                        if (arithmetic.symbol == "$memptr") {
                            lane_scalar = arithmetic.inputs[0];
                            lane_base = arithmetic.inputs[1];
                            lane_offset = arithmetic.immediate;
                        } else if (arithmetic.symbol.empty() &&
                                   (arithmetic_opcode == Opcode::sub_i32 || arithmetic_opcode == Opcode::sub_i64)) {
                            scalar_load_result = arithmetic.inputs[0];
                            if (scalar_load_result >= scalar_loads.size() || !scalar_loads[scalar_load_result].valid ||
                                uses[scalar_load_result] != 1U ||
                                scalar_loads[scalar_load_result].opcode !=
                                    (bytes == 4U ? Opcode::load_ptr_i32 : Opcode::load_ptr_i64)) {
                                legal = false; break;
                            }
                            lane_scalar = arithmetic.inputs[1];
                            lane_base = scalar_loads[scalar_load_result].base;
                            lane_offset = scalar_loads[scalar_load_result].offset;
                        } else {
                            legal = false; break;
                        }
                        if (lane_offset != static_cast<std::int64_t>(lane * bytes)) { legal = false; break; }
                        const auto lane_store_base = store.inputs[1];
                        if (lane == 0U) {
                            base = lane_base;
                            store_base = lane_store_base;
                            scalar = lane_scalar;
                        } else if (base != lane_base || store_base != lane_store_base || scalar != lane_scalar) {
                            legal = false; break;
                        }
                        if (scalar_load_result < function.register_count)
                            candidate_scalar_loads.push_back(scalar_load_result);
                    }
                    if (!legal) continue;
                    if (!slp_profitable({bytes, lanes, 1U, 1U, 1U, 2U, arithmetic_opcode,
                                         SlpMemoryPattern::contiguous_unaligned, 1U, 0U, 1.0}, slp_cost_model, stats)) continue;
                    packed_scalar_loads.insert(candidate_scalar_loads.begin(), candidate_scalar_loads.end());

                    Instruction packed;
                    packed.vector_bits = slp_selected_vector_bits(bytes, lanes, slp_cost_model);
                    const bool inplace = base == store_base;
                    packed.opcode = bytes == 4U
                        ? (inplace ? Opcode::binary_i32_contiguous_inplace : Opcode::binary_i32_contiguous_map)
                        : (inplace ? Opcode::binary_i64_contiguous_inplace : Opcode::binary_i64_contiguous_map);
                    packed.inputs = inplace ? std::vector<VirtualRegister>{base, scalar}
                                            : std::vector<VirtualRegister>{base, store_base, scalar};
                    packed.immediate = static_cast<std::int64_t>(lanes);
                    packed.argument_index = static_cast<std::uint32_t>(arithmetic_opcode);
                    ins[start] = std::move(packed);
                    ins.erase(ins.begin() + static_cast<std::ptrdiff_t>(start + 1U),
                              ins.begin() + static_cast<std::ptrdiff_t>(start + lanes * 2U));
                    matched = true;
                    break;
                }
                (void)matched;
            }
        }
        if (!packed_scalar_loads.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& ins) {
                    return has_result(ins.opcode) && packed_scalar_loads.contains(ins.result);
                }), instructions.end());
            }
        }
    }

    // Recognize packed integer DAGs with shared scalar subexpressions. Unlike
    // the postfix tree form below, this node table gives every packed value an
    // identity so one computed vector can feed multiple parents without being
    // recomputed. Node records are {tag,lhs,rhs} uint16 triples; source tags
    // have bit 15 set and binary nodes reference earlier node IDs.
    {
        std::vector<Instruction*> defs(function.register_count, nullptr);
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        struct LoadInfo { bool valid{}; VirtualRegister base{}; std::int64_t offset{}; Opcode opcode{}; };
        std::vector<LoadInfo> loads(function.register_count);
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                if (has_result(instruction.opcode) && instruction.result < defs.size()) defs[instruction.result] = &instruction;
                for (const auto input : instruction.inputs) if (input < uses.size()) ++uses[input];
                if ((instruction.opcode == Opcode::load_ptr_i32 || instruction.opcode == Opcode::load_ptr_i64) &&
                    instruction.result < loads.size() && instruction.inputs.size() == 1U)
                    loads[instruction.result] = {true, instruction.inputs[0], instruction.immediate, instruction.opcode};
            }
        }
        const auto lane_width = [](Opcode opcode) -> std::size_t {
            switch (opcode) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32: return 4U;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64: return 8U;
            default: return 0U;
            }
        };
        struct Node { std::uint16_t tag{}; std::uint16_t lhs{0xffffU}; std::uint16_t rhs{0xffffU}; };
        std::unordered_set<Instruction*> erase_instructions;
        std::unordered_set<VirtualRegister> erase_results;
        for (auto& block : function.blocks) {
            auto& instructions = block.instructions;
            for (std::size_t start = 0; start < instructions.size(); ++start) {
                for (const std::size_t lanes : {16U, 8U, 4U, 2U}) {
                    if (start + lanes > instructions.size()) continue;
                    const auto store_opcode = instructions[start].opcode;
                    if (store_opcode != Opcode::store_ptr_i32 && store_opcode != Opcode::store_ptr_i64) continue;
                    const std::size_t bytes = store_opcode == Opcode::store_ptr_i32 ? 4U : 8U;
                    const auto load_opcode = bytes == 4U ? Opcode::load_ptr_i32 : Opcode::load_ptr_i64;
                    VirtualRegister destination = function.register_count;
                    std::vector<Node> expected_nodes;
                    std::vector<VirtualRegister> expected_sources;
                    std::vector<Instruction*> local_erase;
                    std::vector<VirtualRegister> local_results;
                    bool legal = true;
                    bool expected_shared = false;
                    for (std::size_t lane = 0; lane < lanes && legal; ++lane) {
                        auto& store = instructions[start + lane];
                        const auto offset = static_cast<std::int64_t>(lane * bytes);
                        if (store.opcode != store_opcode || store.inputs.size() != 2U || store.immediate != offset) { legal = false; break; }
                        if (destination == function.register_count) destination = store.inputs[1];
                        else if (destination != store.inputs[1]) { legal = false; break; }

                        std::vector<Node> nodes;
                        std::vector<VirtualRegister> sources;
                        std::unordered_map<VirtualRegister, std::uint16_t> memo;
                        std::unordered_map<VirtualRegister, std::uint32_t> occurrences;
                        std::unordered_map<VirtualRegister, std::uint16_t> source_nodes;
                        std::unordered_map<VirtualRegister, std::uint16_t> source_indices;
                        std::vector<Instruction*> lane_erase;
                        std::vector<VirtualRegister> lane_results;
                        bool shared = false;
                        std::function<std::optional<std::uint16_t>(VirtualRegister)> visit = [&](VirtualRegister value) -> std::optional<std::uint16_t> {
                            ++occurrences[value];
                            if (const auto it = memo.find(value); it != memo.end()) {
                                if (value < defs.size() && defs[value] != nullptr && lane_width(defs[value]->opcode) == bytes) shared = true;
                                return it->second;
                            }
                            if (value < defs.size() && defs[value] != nullptr && lane_width(defs[value]->opcode) == bytes && defs[value]->inputs.size() == 2U) {
                                auto* operation = defs[value];
                                const auto left = visit(operation->inputs[0]);
                                if (!left) return std::nullopt;
                                std::optional<std::uint16_t> right;
                                if (operation->symbol == "$memptr") {
                                    if (operation->immediate != offset) return std::nullopt;
                                    const auto base = operation->inputs[1];
                                    auto [si, inserted] = source_indices.emplace(base, static_cast<std::uint16_t>(sources.size()));
                                    if (inserted) sources.push_back(base);
                                    auto [sn, new_node] = source_nodes.emplace(base, static_cast<std::uint16_t>(nodes.size()));
                                    if (new_node) nodes.push_back(Node{static_cast<std::uint16_t>(0x8000U | si->second), 0xffffU, 0xffffU});
                                    right = sn->second;
                                } else {
                                    right = visit(operation->inputs[1]);
                                    if (!right) return std::nullopt;
                                }
                                if (nodes.size() >= 0x7fffU) return std::nullopt;
                                const auto id = static_cast<std::uint16_t>(nodes.size());
                                nodes.push_back(Node{static_cast<std::uint16_t>(operation->opcode), *left, *right});
                                memo.emplace(value, id);
                                lane_erase.push_back(operation);
                                lane_results.push_back(value);
                                return id;
                            }
                            if (value >= loads.size() || !loads[value].valid || loads[value].opcode != load_opcode || loads[value].offset != offset)
                                return std::nullopt;
                            const auto base = loads[value].base;
                            auto [si, inserted] = source_indices.emplace(base, static_cast<std::uint16_t>(sources.size()));
                            if (inserted) sources.push_back(base);
                            const auto id = static_cast<std::uint16_t>(nodes.size());
                            nodes.push_back(Node{static_cast<std::uint16_t>(0x8000U | si->second), 0xffffU, 0xffffU});
                            memo.emplace(value, id);
                            if (defs[value] != nullptr) lane_erase.push_back(defs[value]);
                            lane_results.push_back(value);
                            return id;
                        };
                        const auto root = visit(store.inputs[0]);
                        if (!root || *root + 1U != nodes.size() || !shared || nodes.size() < 5U) { legal = false; break; }
                        for (const auto& [value, count] : occurrences) {
                            if (value < uses.size() && uses[value] != count) { legal = false; break; }
                        }
                        if (!legal) break;
                        if (lane == 0U) {
                            expected_nodes = nodes;
                            expected_sources = sources;
                            expected_shared = shared;
                        } else if (expected_nodes.size() != nodes.size() || expected_sources != sources || expected_shared != shared) {
                            legal = false; break;
                        } else {
                            for (std::size_t n = 0; n < nodes.size(); ++n) {
                                if (expected_nodes[n].tag != nodes[n].tag || expected_nodes[n].lhs != nodes[n].lhs || expected_nodes[n].rhs != nodes[n].rhs) {
                                    legal = false; break;
                                }
                            }
                            if (!legal) break;
                        }
                        local_erase.insert(local_erase.end(), lane_erase.begin(), lane_erase.end());
                        local_results.insert(local_results.end(), lane_results.begin(), lane_results.end());
                    }
                    if (!legal || expected_sources.empty()) continue;
                    if (!slp_profitable({bytes, lanes, expected_nodes.size(), expected_sources.size(), 1U, 4U, Opcode::add_i64,
                                         SlpMemoryPattern::contiguous_unaligned, 0U, 0U,
                                         [&] {
                                             double sum = 0.0; std::size_t count = 0U;
                                             for (const auto& node : expected_nodes) if ((node.tag & 0x8000U) == 0U) {
                                                 sum += slp_operation_multiplier(static_cast<Opcode>(node.tag), slp_cost_model); ++count;
                                             }
                                             return count == 0U ? 1.0 : sum / static_cast<double>(count);
                                         }()}, slp_cost_model, stats)) continue;
                    Instruction packed;
                    packed.vector_bits = slp_selected_vector_bits(bytes, lanes, slp_cost_model);
                    packed.opcode = bytes == 4U ? Opcode::binary_i32_contiguous_dag_reuse : Opcode::binary_i64_contiguous_dag_reuse;
                    packed.inputs = expected_sources;
                    packed.inputs.push_back(destination);
                    packed.immediate = static_cast<std::int64_t>(lanes);
                    packed.symbol.reserve(expected_nodes.size() * 6U);
                    const auto append16 = [&](std::uint16_t value) {
                        packed.symbol.push_back(static_cast<char>(value & 0xffU));
                        packed.symbol.push_back(static_cast<char>((value >> 8U) & 0xffU));
                    };
                    for (const auto& node : expected_nodes) { append16(node.tag); append16(node.lhs); append16(node.rhs); }
                    instructions[start] = std::move(packed);
                    instructions.erase(instructions.begin() + static_cast<std::ptrdiff_t>(start + 1U),
                                       instructions.begin() + static_cast<std::ptrdiff_t>(start + lanes));
                    for (auto* instruction : local_erase) erase_instructions.insert(instruction);
                    for (const auto value : local_results) erase_results.insert(value);
                    break;
                }
            }
        }
        if (!erase_instructions.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& instruction) {
                    return erase_instructions.contains(const_cast<Instruction*>(&instruction)) ||
                           (has_result(instruction.opcode) && erase_results.contains(instruction.result));
                }), instructions.end());
            }
        }
    }

    // Recognize true branching packed expression DAGs over contiguous source
    // arrays. The scalar tree is serialized as a compact postfix program in
    // Instruction::symbol; source leaves reference Instruction::inputs by
    // index. This supports arbitrary binary tree shapes without a fixed depth
    // of map2/map3/mapN pseudo-ops.
    {
        std::vector<Instruction*> defs(function.register_count, nullptr);
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        struct LoadInfo { bool valid{}; VirtualRegister base{}; std::int64_t offset{}; Opcode opcode{}; };
        std::vector<LoadInfo> loads(function.register_count);
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                if (has_result(instruction.opcode) && instruction.result < defs.size()) defs[instruction.result] = &instruction;
                for (const auto input : instruction.inputs) if (input < uses.size()) ++uses[input];
                if ((instruction.opcode == Opcode::load_ptr_i32 || instruction.opcode == Opcode::load_ptr_i64) &&
                    instruction.result < loads.size() && instruction.inputs.size() == 1U)
                    loads[instruction.result] = {true, instruction.inputs[0], instruction.immediate, instruction.opcode};
            }
        }
        const auto lane_width = [](Opcode opcode) -> std::size_t {
            switch (opcode) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32: return 4U;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64: return 8U;
            default: return 0U;
            }
        };
        struct DagBuild {
            bool legal{true};
            bool contains_operation{};
            bool branching{};
            std::vector<std::uint16_t> tokens;
            std::vector<VirtualRegister> sources;
            std::vector<Instruction*> erase;
            std::vector<VirtualRegister> erase_results;
            std::size_t operation_count{};
            std::size_t max_stack_depth{};
        };
        std::unordered_set<Instruction*> erase_instructions;
        std::unordered_set<VirtualRegister> erase_results;
        for (auto& block : function.blocks) {
            auto& instructions = block.instructions;
            for (std::size_t start = 0; start < instructions.size(); ++start) {
                for (const std::size_t lanes : {16U, 8U, 4U, 2U}) {
                    if (start + lanes > instructions.size()) continue;
                    const auto store_opcode = instructions[start].opcode;
                    if (store_opcode != Opcode::store_ptr_i32 && store_opcode != Opcode::store_ptr_i64) continue;
                    const std::size_t bytes = store_opcode == Opcode::store_ptr_i32 ? 4U : 8U;
                    const auto load_opcode = bytes == 4U ? Opcode::load_ptr_i32 : Opcode::load_ptr_i64;
                    VirtualRegister destination = function.register_count;
                    std::vector<std::uint16_t> expected_tokens;
                    std::vector<VirtualRegister> expected_sources;
                    std::vector<Instruction*> local_erase;
                    std::vector<VirtualRegister> local_results;
                    bool legal = true;
                    bool expected_branching = false;
                    std::size_t expected_ops = 0U;
                    std::size_t expected_max_depth = 0U;
                    for (std::size_t lane = 0; lane < lanes && legal; ++lane) {
                        auto& store = instructions[start + lane];
                        const auto offset = static_cast<std::int64_t>(lane * bytes);
                        if (store.opcode != store_opcode || store.inputs.size() != 2U || store.immediate != offset) { legal = false; break; }
                        if (destination == function.register_count) destination = store.inputs[1];
                        else if (destination != store.inputs[1]) { legal = false; break; }

                        DagBuild build;
                        std::unordered_map<VirtualRegister, std::uint16_t> source_indices;
                        std::function<bool(VirtualRegister)> visit = [&](VirtualRegister value) -> bool {
                            if (value < defs.size() && defs[value] != nullptr && uses[value] == 1U &&
                                lane_width(defs[value]->opcode) == bytes && defs[value]->inputs.size() == 2U) {
                                auto* operation = defs[value];
                                const auto left = operation->inputs[0];
                                const bool left_is_op = left < defs.size() && defs[left] != nullptr && uses[left] == 1U && lane_width(defs[left]->opcode) == bytes;
                                if (!visit(left)) return false;
                                bool right_is_op = false;
                                if (operation->symbol == "$memptr") {
                                    if (operation->immediate != offset) return false;
                                    const auto base = operation->inputs[1];
                                    auto [it, inserted] = source_indices.emplace(base, static_cast<std::uint16_t>(build.sources.size()));
                                    if (inserted) build.sources.push_back(base);
                                    build.tokens.push_back(static_cast<std::uint16_t>(0x8000U | it->second));
                                } else {
                                    const auto right = operation->inputs[1];
                                    right_is_op = right < defs.size() && defs[right] != nullptr && uses[right] == 1U && lane_width(defs[right]->opcode) == bytes;
                                    if (!visit(right)) return false;
                                }
                                build.tokens.push_back(static_cast<std::uint16_t>(operation->opcode));
                                build.erase.push_back(operation);
                                build.erase_results.push_back(operation->result);
                                ++build.operation_count;
                                build.contains_operation = true;
                                if (left_is_op && right_is_op) build.branching = true;
                                return true;
                            }
                            if (value >= loads.size() || !loads[value].valid || loads[value].opcode != load_opcode ||
                                loads[value].offset != offset || uses[value] != 1U) return false;
                            const auto base = loads[value].base;
                            auto [it, inserted] = source_indices.emplace(base, static_cast<std::uint16_t>(build.sources.size()));
                            if (inserted) build.sources.push_back(base);
                            build.tokens.push_back(static_cast<std::uint16_t>(0x8000U | it->second));
                            if (defs[value] != nullptr) build.erase.push_back(defs[value]);
                            build.erase_results.push_back(value);
                            return true;
                        };
                        const auto result = store.inputs[0];
                        if (!visit(result) || !build.branching || build.operation_count < 3U) { legal = false; break; }
                        std::size_t depth = 0U;
                        for (const auto token : build.tokens) {
                            if ((token & 0x8000U) != 0U) { ++depth; build.max_stack_depth = std::max(build.max_stack_depth, depth); }
                            else { if (depth < 2U) { legal = false; break; } --depth; }
                        }
                        if (!legal || depth != 1U || build.max_stack_depth > 8U) { legal = false; break; }
                        if (lane == 0U) {
                            expected_tokens = build.tokens;
                            expected_sources = build.sources;
                            expected_branching = build.branching;
                            expected_ops = build.operation_count;
                            expected_max_depth = build.max_stack_depth;
                        } else if (expected_tokens != build.tokens || expected_sources != build.sources ||
                                   expected_branching != build.branching || expected_ops != build.operation_count ||
                                   expected_max_depth != build.max_stack_depth) { legal = false; break; }
                        local_erase.insert(local_erase.end(), build.erase.begin(), build.erase.end());
                        local_results.insert(local_results.end(), build.erase_results.begin(), build.erase_results.end());
                    }
                    if (!legal || expected_sources.empty()) continue;
                    if (!slp_profitable({bytes, lanes, expected_ops, expected_sources.size(), 1U, expected_max_depth, Opcode::add_i64,
                                         SlpMemoryPattern::contiguous_unaligned, 0U, 0U,
                                         [&] {
                                             double sum = 0.0; std::size_t count = 0U;
                                             for (const auto token : expected_tokens) if ((token & 0x8000U) == 0U) {
                                                 sum += slp_operation_multiplier(static_cast<Opcode>(token), slp_cost_model); ++count;
                                             }
                                             return count == 0U ? 1.0 : sum / static_cast<double>(count);
                                         }()}, slp_cost_model, stats)) continue;
                    Instruction packed;
                    packed.vector_bits = slp_selected_vector_bits(bytes, lanes, slp_cost_model);
                    packed.opcode = bytes == 4U ? Opcode::binary_i32_contiguous_dag : Opcode::binary_i64_contiguous_dag;
                    packed.inputs = expected_sources;
                    packed.inputs.push_back(destination);
                    packed.immediate = static_cast<std::int64_t>(lanes);
                    packed.symbol.reserve(expected_tokens.size() * 2U);
                    for (const auto token : expected_tokens) {
                        packed.symbol.push_back(static_cast<char>(token & 0xffU));
                        packed.symbol.push_back(static_cast<char>((token >> 8U) & 0xffU));
                    }
                    instructions[start] = std::move(packed);
                    instructions.erase(instructions.begin() + static_cast<std::ptrdiff_t>(start + 1U),
                                       instructions.begin() + static_cast<std::ptrdiff_t>(start + lanes));
                    for (auto* instruction : local_erase) erase_instructions.insert(instruction);
                    for (const auto value : local_results) erase_results.insert(value);
                    break;
                }
            }
        }
        if (!erase_instructions.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& instruction) {
                    return erase_instructions.contains(const_cast<Instruction*>(&instruction)) ||
                           (has_result(instruction.opcode) && erase_results.contains(instruction.result));
                }), instructions.end());
            }
        }
    }

    // Recognize arbitrary-depth left-deep vector expression chains over
    // contiguous source arrays and one contiguous destination array:
    //
    //     dst[i] = (((a[i] op0 b[i]) op1 c[i]) op2 d[i]) ...
    //
    // This deliberately requires every scalar load and intermediate to be
    // single-use and every source to use the exact same contiguous lane
    // offsets. The chain length is carried explicitly on the machine
    // instruction, so this optimization has no fixed operation-count ceiling.
    {
        std::vector<Instruction*> defs(function.register_count, nullptr);
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        struct LoadInfo { bool valid{}; VirtualRegister base{}; std::int64_t offset{}; Opcode opcode{}; };
        std::vector<LoadInfo> loads(function.register_count);
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                if (has_result(instruction.opcode) && instruction.result < defs.size()) defs[instruction.result] = &instruction;
                for (const auto input : instruction.inputs) if (input < uses.size()) ++uses[input];
                if ((instruction.opcode == Opcode::load_ptr_i32 || instruction.opcode == Opcode::load_ptr_i64) &&
                    instruction.result < loads.size() && instruction.inputs.size() == 1U)
                    loads[instruction.result] = {true, instruction.inputs[0], instruction.immediate, instruction.opcode};
            }
        }
        const auto lane_width = [](Opcode opcode) -> std::size_t {
            switch (opcode) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32: return 4U;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64: return 8U;
            default: return 0U;
            }
        };
        std::unordered_set<Instruction*> erase_instructions;
        std::unordered_set<VirtualRegister> erase_results;
        for (auto& block : function.blocks) {
            auto& instructions = block.instructions;
            for (std::size_t start = 0; start < instructions.size(); ++start) {
                for (const std::size_t lanes : {16U, 8U, 4U, 2U}) {
                    if (start + lanes > instructions.size()) continue;
                    const auto store_opcode = instructions[start].opcode;
                    if (store_opcode != Opcode::store_ptr_i32 && store_opcode != Opcode::store_ptr_i64) continue;
                    const std::size_t bytes = store_opcode == Opcode::store_ptr_i32 ? 4U : 8U;
                    const auto load_opcode = bytes == 4U ? Opcode::load_ptr_i32 : Opcode::load_ptr_i64;
                    VirtualRegister destination = function.register_count;
                    std::vector<VirtualRegister> expected_sources;
                    std::vector<Opcode> expected_operations;
                    std::vector<Instruction*> local_erase;
                    std::vector<VirtualRegister> local_results;
                    bool legal = true;
                    for (std::size_t lane = 0; lane < lanes && legal; ++lane) {
                        auto& store = instructions[start + lane];
                        const auto offset = static_cast<std::int64_t>(lane * bytes);
                        if (store.opcode != store_opcode || store.inputs.size() != 2U || store.immediate != offset) { legal = false; break; }
                        if (destination == function.register_count) destination = store.inputs[1];
                        else if (destination != store.inputs[1]) { legal = false; break; }
                        auto result = store.inputs[0];
                        if (result >= defs.size() || defs[result] == nullptr || uses[result] != 1U) { legal = false; break; }

                        std::vector<Opcode> reversed_operations;
                        std::vector<VirtualRegister> reversed_right_sources;
                        std::vector<Instruction*> lane_erase;
                        std::vector<VirtualRegister> lane_results;
                        VirtualRegister first_source = function.register_count;
                        auto* operation = defs[result];
                        while (operation != nullptr && lane_width(operation->opcode) == bytes && operation->inputs.size() == 2U) {
                            reversed_operations.push_back(operation->opcode);
                            lane_erase.push_back(operation);
                            lane_results.push_back(operation->result);

                            VirtualRegister right_base = function.register_count;
                            if (operation->symbol == "$memptr") {
                                if (operation->immediate != offset) { legal = false; break; }
                                right_base = operation->inputs[1];
                            } else {
                                const auto right = operation->inputs[1];
                                if (right >= loads.size() || !loads[right].valid || loads[right].opcode != load_opcode ||
                                    loads[right].offset != offset || uses[right] != 1U) { legal = false; break; }
                                right_base = loads[right].base;
                                if (defs[right] != nullptr) lane_erase.push_back(defs[right]);
                                lane_results.push_back(right);
                            }
                            reversed_right_sources.push_back(right_base);

                            const auto left = operation->inputs[0];
                            if (left < defs.size() && defs[left] != nullptr && lane_width(defs[left]->opcode) == bytes && uses[left] == 1U) {
                                operation = defs[left];
                                continue;
                            }
                            if (left >= loads.size() || !loads[left].valid || loads[left].opcode != load_opcode ||
                                loads[left].offset != offset || uses[left] != 1U) { legal = false; break; }
                            first_source = loads[left].base;
                            if (defs[left] != nullptr) lane_erase.push_back(defs[left]);
                            lane_results.push_back(left);
                            operation = nullptr;
                        }
                        if (!legal || first_source == function.register_count || reversed_operations.size() < 3U) { legal = false; break; }

                        std::reverse(reversed_operations.begin(), reversed_operations.end());
                        std::reverse(reversed_right_sources.begin(), reversed_right_sources.end());
                        std::vector<VirtualRegister> lane_sources;
                        lane_sources.reserve(reversed_right_sources.size() + 1U);
                        lane_sources.push_back(first_source);
                        lane_sources.insert(lane_sources.end(), reversed_right_sources.begin(), reversed_right_sources.end());
                        if (lane == 0U) {
                            expected_operations = reversed_operations;
                            expected_sources = lane_sources;
                        } else if (expected_operations != reversed_operations || expected_sources != lane_sources) {
                            legal = false; break;
                        }
                        local_erase.insert(local_erase.end(), lane_erase.begin(), lane_erase.end());
                        local_results.insert(local_results.end(), lane_results.begin(), lane_results.end());
                    }
                    if (!legal || expected_operations.size() < 3U || destination == function.register_count) continue;
                    if (!slp_profitable({bytes, lanes, expected_operations.size(), expected_sources.size(), 1U, 3U,
                                         expected_operations.empty() ? Opcode::add_i64 : expected_operations.front(),
                                         SlpMemoryPattern::contiguous_unaligned, 0U, 0U,
                                         [&] {
                                             if (expected_operations.empty()) return 1.0;
                                             const auto base = slp_operation_multiplier(expected_operations.front(), slp_cost_model);
                                             double sum = 0.0;
                                             for (const auto op : expected_operations) sum += slp_operation_multiplier(op, slp_cost_model);
                                             return sum / (static_cast<double>(expected_operations.size()) * base);
                                         }()}, slp_cost_model, stats)) continue;

                    Instruction packed;
                    packed.vector_bits = slp_selected_vector_bits(bytes, lanes, slp_cost_model);
                    packed.opcode = bytes == 4U ? Opcode::binary_i32_contiguous_chain : Opcode::binary_i64_contiguous_chain;
                    packed.inputs = expected_sources;
                    packed.inputs.push_back(destination);
                    packed.immediate = static_cast<std::int64_t>(lanes);
                    packed.symbol.clear();
                    packed.symbol.reserve(expected_operations.size() * 2U);
                    for (const auto operation : expected_operations) {
                        const auto encoded = static_cast<std::uint32_t>(operation);
                        packed.symbol.push_back(static_cast<char>(encoded & 0xffU));
                        packed.symbol.push_back(static_cast<char>((encoded >> 8U) & 0xffU));
                    }
                    instructions[start] = std::move(packed);
                    for (std::size_t lane = 1U; lane < lanes; ++lane) erase_instructions.insert(&instructions[start + lane]);
                    for (auto* instruction : local_erase) erase_instructions.insert(instruction);
                    for (const auto value : local_results) erase_results.insert(value);
                    break;
                }
            }
        }
        if (!erase_instructions.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& instruction) {
                    return erase_instructions.contains(const_cast<Instruction*>(&instruction)) ||
                           (has_result(instruction.opcode) && erase_results.contains(instruction.result));
                }), instructions.end());
            }
        }
    }

    // Recognize two-stage vector expression chains over three contiguous
    // source arrays and one destination array:
    //
    //     dst[i] = (a[i] op1 b[i]) op2 c[i]
    //
    // The intermediate is kept entirely in the packed register domain. Both
    // operations are ordered lane-wise ADD/SUB/AND/OR/XOR for i32/i64.
    // Every scalar load/intermediate/final result must be single-use and the
    // destination stores must form one exact contiguous group.
    {
        std::vector<Instruction*> defs(function.register_count, nullptr);
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        struct LoadInfo { bool valid{}; VirtualRegister base{}; std::int64_t offset{}; Opcode opcode{}; };
        std::vector<LoadInfo> loads(function.register_count);
        for (auto& b : function.blocks) for (auto& x : b.instructions) {
            if (has_result(x.opcode) && x.result < defs.size()) defs[x.result] = &x;
            for (auto r : x.inputs) if (r < uses.size()) ++uses[r];
            if ((x.opcode == Opcode::load_ptr_i32 || x.opcode == Opcode::load_ptr_i64) && x.result < loads.size() && x.inputs.size()==1U)
                loads[x.result] = {true, x.inputs[0], x.immediate, x.opcode};
        }
        const auto lane_width=[](Opcode op)->std::size_t {
            switch(op){case Opcode::add_i32:case Opcode::sub_i32:case Opcode::and_i32:case Opcode::or_i32:case Opcode::xor_i32:return 4U;
            case Opcode::add_i64:case Opcode::sub_i64:case Opcode::and_i64:case Opcode::or_i64:case Opcode::xor_i64:return 8U;default:return 0U;}
        };
        std::unordered_set<Instruction*> erase_ins;
        std::unordered_set<VirtualRegister> erase_results;
        for (auto& b : function.blocks) {
            auto& v=b.instructions;
            for(std::size_t start=0; start<v.size(); ++start){
                for(std::size_t lanes: {16U,8U,4U,2U}){
                    if(start+lanes>v.size()) continue;
                    Opcode storeop=v[start].opcode;
                    if(storeop!=Opcode::store_ptr_i32 && storeop!=Opcode::store_ptr_i64) continue;
                    const std::size_t bytes=storeop==Opcode::store_ptr_i32?4U:8U;
                    const Opcode loadop=bytes==4U?Opcode::load_ptr_i32:Opcode::load_ptr_i64;
                    VirtualRegister abase=function.register_count,bbase=function.register_count,cbase=function.register_count,dst=function.register_count;
                    Opcode op1=Opcode::jump,op2=Opcode::jump; bool legal=true;
                    std::vector<Instruction*> local_erase;
                    std::vector<VirtualRegister> local_results;
                    auto decode_load=[&](Instruction* op, std::size_t input_index, std::int64_t off, VirtualRegister& base, std::vector<Instruction*>& ei, std::vector<VirtualRegister>& er)->bool{
                        if(input_index>=op->inputs.size()) return false;
                        auto r=op->inputs[input_index];
                        if(r>=loads.size()||!loads[r].valid||loads[r].opcode!=loadop||loads[r].offset!=off||uses[r]!=1U) return false;
                        if(base==function.register_count) base=loads[r].base; else if(base!=loads[r].base) return false;
                        if (defs[r]) ei.push_back(defs[r]);
                        er.push_back(r);
                        return true;
                    };
                    for(std::size_t lane=0; lane<lanes && legal; ++lane){
                        auto& st=v[start+lane]; const auto off=static_cast<std::int64_t>(lane*bytes);
                        if(st.opcode!=storeop||st.inputs.size()!=2U||st.immediate!=off){legal=false;break;}
                        if(dst==function.register_count) dst=st.inputs[1]; else if(dst!=st.inputs[1]){legal=false;break;}
                        auto finalr=st.inputs[0]; if(finalr>=defs.size()||!defs[finalr]||uses[finalr]!=1U){legal=false;break;}
                        auto* second=defs[finalr]; auto w2=lane_width(second->opcode); if(w2!=bytes||second->inputs.size()!=2U){legal=false;break;}
                        if(lane==0)op2=second->opcode; else if(op2!=second->opcode){legal=false;break;}
                        Instruction* first=nullptr;
                        // Prefer the non-memory first operand as the intermediate.
                        auto mid=second->inputs[0];
                        if(mid<defs.size()) first=defs[mid];
                        if(!first||lane_width(first->opcode)!=bytes||uses[mid]!=1U){legal=false;break;}
                        if(lane==0)op1=first->opcode; else if(op1!=first->opcode){legal=false;break;}
                        if(first->inputs.size()!=2U){legal=false;break;}
                        // op1: A plus B, allowing B to have been folded as $memptr.
                        if(!decode_load(first,0,off,abase,local_erase,local_results)){legal=false;break;}
                        if(first->symbol=="$memptr") { if(first->immediate!=off){legal=false;break;} auto bb=first->inputs[1]; if(bbase==function.register_count)bbase=bb; else if(bbase!=bb){legal=false;break;} }
                        else if(!decode_load(first,1,off,bbase,local_erase,local_results)){legal=false;break;}
                        // op2: intermediate plus C, allowing C as $memptr.
                        if(second->symbol=="$memptr") { if(second->immediate!=off){legal=false;break;} auto cb=second->inputs[1]; if(cbase==function.register_count)cbase=cb; else if(cbase!=cb){legal=false;break;} }
                        else if(!decode_load(second,1,off,cbase,local_erase,local_results)){legal=false;break;}
                        local_erase.push_back(first); local_erase.push_back(second); local_results.push_back(mid); local_results.push_back(finalr);
                    }
                    if(!legal||abase==function.register_count||bbase==function.register_count||cbase==function.register_count||dst==function.register_count) continue;
                    if (!slp_profitable({bytes, lanes, 2U, 3U, 1U, 4U, op1,
                                         SlpMemoryPattern::contiguous_unaligned, 0U, 0U,
                                         (slp_operation_multiplier(op1, slp_cost_model) + slp_operation_multiplier(op2, slp_cost_model)) /
                                             (2.0 * slp_operation_multiplier(op1, slp_cost_model))}, slp_cost_model, stats)) continue;
                    Instruction packed; packed.opcode=bytes==4U?Opcode::binary_i32_contiguous_map3:Opcode::binary_i64_contiguous_map3;
                    packed.vector_bits = slp_selected_vector_bits(bytes, lanes, slp_cost_model);
                    packed.inputs={abase,bbase,cbase,dst}; packed.immediate=static_cast<std::int64_t>(lanes);
                    packed.argument_index=(static_cast<std::uint32_t>(op1)&0xffffU)|((static_cast<std::uint32_t>(op2)&0xffffU)<<16U);
                    v[start]=std::move(packed);
                    for(std::size_t lane=1;lane<lanes;++lane) erase_ins.insert(&v[start+lane]);
                    for (auto* x : local_erase) erase_ins.insert(x);
                    for (auto r : local_results) erase_results.insert(r);
                    break;
                }
            }
        }
        if(!erase_ins.empty()) for(auto& b:function.blocks){ auto& v=b.instructions; v.erase(std::remove_if(v.begin(),v.end(),[&](Instruction const& x){return erase_ins.contains(const_cast<Instruction*>(&x)) || (has_result(x.opcode)&&erase_results.contains(x.result));}),v.end()); }
    }

    // Recognize vector-to-vector integer SLP maps over two contiguous
    // source arrays and one contiguous destination array:
    //
    //     dst[i] = lhs[i] op rhs[i]
    //
    // The operation is lane-wise ADD/SUB/AND/OR/XOR for i32/i64. Every
    // scalar load and arithmetic result must be single-use, offsets must be
    // exact and contiguous, and arithmetic/store groups must be adjacent.
    // This preserves scalar alias/ordering semantics while allowing the x86
    // backend to issue true vector-to-vector SSE2 operations.
    {
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        struct ScalarLoadInfo { bool valid{}; VirtualRegister base{}; std::int64_t offset{}; Opcode opcode{}; };
        std::vector<ScalarLoadInfo> scalar_loads(function.register_count);
        for (const auto& block : function.blocks) {
            for (const auto& ins : block.instructions) {
                for (const auto input : ins.inputs) if (input < uses.size()) ++uses[input];
                if ((ins.opcode == Opcode::load_ptr_i32 || ins.opcode == Opcode::load_ptr_i64) &&
                    ins.result < scalar_loads.size() && ins.inputs.size() == 1U)
                    scalar_loads[ins.result] = {true, ins.inputs[0], ins.immediate, ins.opcode};
            }
        }
        std::unordered_set<VirtualRegister> erased_loads;
        const auto lane_width = [](Opcode opcode) -> std::size_t {
            switch (opcode) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32: return 4U;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64: return 8U;
            default: return 0U;
            }
        };
        const auto matching_store = [](Opcode arithmetic) -> Opcode {
            switch (arithmetic) {
            case Opcode::add_i32: case Opcode::sub_i32: case Opcode::and_i32: case Opcode::or_i32: case Opcode::xor_i32: return Opcode::store_ptr_i32;
            case Opcode::add_i64: case Opcode::sub_i64: case Opcode::and_i64: case Opcode::or_i64: case Opcode::xor_i64: return Opcode::store_ptr_i64;
            default: return Opcode::jump;
            }
        };
        for (auto& block : function.blocks) {
            auto& ins = block.instructions;
            for (std::size_t start = 0; start < ins.size(); ++start) {
                for (const std::size_t lanes : {16U, 8U, 4U, 2U}) {
                    if (start + lanes * 2U > ins.size()) continue;
                    const auto arithmetic_opcode = ins[start].opcode;
                    const auto bytes = lane_width(arithmetic_opcode);
                    if (bytes == 0U) continue;
                    const auto load_opcode = bytes == 4U ? Opcode::load_ptr_i32 : Opcode::load_ptr_i64;
                    const auto store_opcode = matching_store(arithmetic_opcode);
                    VirtualRegister lhs_base = function.register_count;
                    VirtualRegister rhs_base = function.register_count;
                    VirtualRegister dst_base = function.register_count;
                    bool legal = true;
                    std::vector<VirtualRegister> candidate_loads;
                    for (std::size_t lane = 0; lane < lanes; ++lane) {
                        const auto& arithmetic = ins[start + lane];
                        const auto& store = ins[start + lanes + lane];
                        if (arithmetic.opcode != arithmetic_opcode || arithmetic.inputs.size() != 2U ||
                            store.opcode != store_opcode || store.inputs.size() != 2U ||
                            store.inputs[0] != arithmetic.result || arithmetic.result >= uses.size() ||
                            uses[arithmetic.result] != 1U ||
                            store.immediate != static_cast<std::int64_t>(lane * bytes)) {
                            legal = false; break;
                        }
                        VirtualRegister lane_lhs_base = function.register_count;
                        VirtualRegister lane_rhs_base = function.register_count;
                        VirtualRegister lhs_load = function.register_count;
                        VirtualRegister rhs_load = function.register_count;
                        const auto expected_offset = static_cast<std::int64_t>(lane * bytes);
                        if (arithmetic.symbol == "$memptr") {
                            // Existing scalar memory folding leaves the first
                            // array as a scalar load and encodes the second
                            // directly as [base+offset].
                            lhs_load = arithmetic.inputs[0];
                            if (lhs_load >= scalar_loads.size() || !scalar_loads[lhs_load].valid ||
                                scalar_loads[lhs_load].opcode != load_opcode || uses[lhs_load] != 1U ||
                                scalar_loads[lhs_load].offset != expected_offset || arithmetic.immediate != expected_offset) {
                                legal = false; break;
                            }
                            lane_lhs_base = scalar_loads[lhs_load].base;
                            lane_rhs_base = arithmetic.inputs[1];
                        } else if (arithmetic.symbol.empty()) {
                            lhs_load = arithmetic.inputs[0];
                            rhs_load = arithmetic.inputs[1];
                            if (lhs_load >= scalar_loads.size() || rhs_load >= scalar_loads.size() ||
                                !scalar_loads[lhs_load].valid || !scalar_loads[rhs_load].valid ||
                                scalar_loads[lhs_load].opcode != load_opcode || scalar_loads[rhs_load].opcode != load_opcode ||
                                uses[lhs_load] != 1U || uses[rhs_load] != 1U ||
                                scalar_loads[lhs_load].offset != expected_offset || scalar_loads[rhs_load].offset != expected_offset) {
                                legal = false; break;
                            }
                            lane_lhs_base = scalar_loads[lhs_load].base;
                            lane_rhs_base = scalar_loads[rhs_load].base;
                        } else {
                            legal = false; break;
                        }
                        if (lane == 0U) {
                            lhs_base = lane_lhs_base;
                            rhs_base = lane_rhs_base;
                            dst_base = store.inputs[1];
                        } else if (lhs_base != lane_lhs_base || rhs_base != lane_rhs_base || dst_base != store.inputs[1]) {
                            legal = false; break;
                        }
                        candidate_loads.push_back(lhs_load);
                        if (rhs_load < function.register_count) candidate_loads.push_back(rhs_load);
                    }
                    if (!legal || lhs_base == function.register_count || rhs_base == function.register_count || dst_base == function.register_count)
                        continue;
                    if (!slp_profitable({bytes, lanes, 1U, 2U, 1U, 3U, arithmetic_opcode,
                                         SlpMemoryPattern::contiguous_unaligned, 0U, 0U, 1.0}, slp_cost_model, stats)) continue;
                    erased_loads.insert(candidate_loads.begin(), candidate_loads.end());
                    Instruction packed;
                    packed.vector_bits = slp_selected_vector_bits(bytes, lanes, slp_cost_model);
                    packed.opcode = bytes == 4U ? Opcode::binary_i32_contiguous_map2 : Opcode::binary_i64_contiguous_map2;
                    packed.inputs = {lhs_base, rhs_base, dst_base};
                    packed.immediate = static_cast<std::int64_t>(lanes);
                    packed.argument_index = static_cast<std::uint32_t>(arithmetic_opcode);
                    ins[start] = std::move(packed);
                    ins.erase(ins.begin() + static_cast<std::ptrdiff_t>(start + 1U),
                              ins.begin() + static_cast<std::ptrdiff_t>(start + lanes * 2U));
                    break;
                }
            }
        }
        if (!erased_loads.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& ins) {
                    return has_result(ins.opcode) && erased_loads.contains(ins.result);
                }), instructions.end());
            }
        }
    }

    // Recognize straight-line i32 addition reductions over contiguous
    // 4-byte loads. Like the i64 SLP path below, this is legality-first and
    // only fires for exact single-use reduction trees with one common base.
    {
        std::vector<Instruction*> defs(function.register_count, nullptr);
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        for (auto& block : function.blocks) {
            for (auto& ins : block.instructions) {
                if (has_result(ins.opcode) && ins.result < defs.size()) defs[ins.result] = &ins;
                for (auto input : ins.inputs) if (input < uses.size()) ++uses[input];
            }
        }

        struct ReductionLeaf { VirtualRegister base{}; std::int64_t offset{}; };
        std::unordered_set<VirtualRegister> slp_erased_results;
        for (auto& block : function.blocks) {
            for (auto& ret : block.instructions) {
                if (ret.opcode != Opcode::return_i32 || ret.inputs.size() != 1U) continue;
                const auto root = ret.inputs.front();
                std::vector<ReductionLeaf> leaves;
                std::vector<VirtualRegister> visited;
                const auto collect = [&](auto&& self, VirtualRegister reg) -> bool {
                    if (reg >= defs.size() || defs[reg] == nullptr) return false;
                    auto* def = defs[reg];
                    if (def->opcode == Opcode::load_ptr_i32 && def->inputs.size() == 1U) {
                        leaves.push_back({def->inputs[0], def->immediate});
                        visited.push_back(reg);
                        return true;
                    }
                    if (def->opcode != Opcode::add_i32 || uses[reg] != 1U) return false;
                    if (def->symbol == "$memptr") {
                        if (def->inputs.size() != 2U) return false;
                        if (!self(self, def->inputs[0])) return false;
                        leaves.push_back({def->inputs[1], def->immediate});
                        visited.push_back(reg);
                        return true;
                    }
                    if (!def->symbol.empty() || def->inputs.size() != 2U) return false;
                    if (!self(self, def->inputs[0]) || !self(self, def->inputs[1])) return false;
                    visited.push_back(reg);
                    return true;
                };
                if (!collect(collect, root)) continue;
                if (leaves.size() < 4U || leaves.size() > 32U || (leaves.size() & (leaves.size() - 1U)) != 0U) continue;
                const auto base = leaves.front().base;
                bool legal = true;
                std::vector<std::int64_t> offsets; offsets.reserve(leaves.size());
                for (const auto& leaf : leaves) {
                    if (leaf.base != base) { legal = false; break; }
                    offsets.push_back(leaf.offset);
                }
                if (!legal) continue;
                std::sort(offsets.begin(), offsets.end());
                for (std::size_t i = 0; i < offsets.size(); ++i)
                    if (offsets[i] != static_cast<std::int64_t>(i * 4U)) { legal = false; break; }
                if (!legal) continue;

                auto* root_def = defs[root];
                root_def->opcode = Opcode::reduce_add_i32_contiguous;
                root_def->inputs = {base};
                root_def->immediate = static_cast<std::int64_t>(leaves.size());
                root_def->symbol.clear();
                for (const auto reg : visited) if (reg != root) slp_erased_results.insert(reg);
                break;
            }
        }
        if (!slp_erased_results.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& ins) {
                    return has_result(ins.opcode) && slp_erased_results.contains(ins.result);
                }), instructions.end());
            }
        }
    }

    // Recognize a straight-line i64 addition reduction whose leaves are
    // contiguous 8-byte loads from the same base pointer. Represent it as a
    // single machine pseudo-op so the target encoder can use packed SSE2
    // loads/adds without introducing vector types into the public Forge IR.
    // This is deliberately legality-first: all reduction nodes must be
    // single-use, every leaf must be a plain pointer load, and offsets must
    // form an exact 0,8,... sequence.
    {
        std::vector<Instruction*> defs(function.register_count, nullptr);
        std::vector<std::uint32_t> uses(function.register_count, 0U);
        for (auto& block : function.blocks) {
            for (auto& ins : block.instructions) {
                if (has_result(ins.opcode) && ins.result < defs.size()) defs[ins.result] = &ins;
                for (auto input : ins.inputs) if (input < uses.size()) ++uses[input];
            }
        }

        struct ReductionLeaf { VirtualRegister base{}; std::int64_t offset{}; };
        std::unordered_set<VirtualRegister> slp_erased_results;
        for (auto& block : function.blocks) {
            for (auto& ret : block.instructions) {
                if (ret.opcode != Opcode::return_i64 || ret.inputs.size() != 1U) continue;
                const auto root = ret.inputs.front();
                std::vector<ReductionLeaf> leaves;
                std::vector<VirtualRegister> visited;
                const auto collect = [&](auto&& self, VirtualRegister reg) -> bool {
                    if (reg >= defs.size() || defs[reg] == nullptr) return false;
                    auto* def = defs[reg];
                    if (def->opcode == Opcode::load_ptr_i64 && def->inputs.size() == 1U) {
                        leaves.push_back({def->inputs[0], def->immediate});
                        visited.push_back(reg);
                        return true;
                    }
                    if (def->opcode != Opcode::add_i64 || uses[reg] != 1U) return false;
                    if (def->symbol == "$memptr") {
                        if (def->inputs.size() != 2U) return false;
                        if (!self(self, def->inputs[0])) return false;
                        leaves.push_back({def->inputs[1], def->immediate});
                        visited.push_back(reg);
                        return true;
                    }
                    if (!def->symbol.empty() || def->inputs.size() != 2U) return false;
                    if (!self(self, def->inputs[0]) || !self(self, def->inputs[1])) return false;
                    visited.push_back(reg);
                    return true;
                };
                if (!collect(collect, root)) continue;
                if (leaves.size() < 4U || leaves.size() > 16U || (leaves.size() & (leaves.size() - 1U)) != 0U) continue;
                const auto base = leaves.front().base;
                bool legal = true;
                std::vector<std::int64_t> offsets; offsets.reserve(leaves.size());
                for (const auto& leaf : leaves) {
                    if (leaf.base != base) { legal = false; break; }
                    offsets.push_back(leaf.offset);
                }
                if (!legal) continue;
                std::sort(offsets.begin(), offsets.end());
                for (std::size_t i = 0; i < offsets.size(); ++i)
                    if (offsets[i] != static_cast<std::int64_t>(i * 8U)) { legal = false; break; }
                if (!legal) continue;

                auto* root_def = defs[root];
                root_def->opcode = Opcode::reduce_add_i64_contiguous;
                root_def->inputs = {base};
                root_def->immediate = static_cast<std::int64_t>(leaves.size());
                root_def->symbol.clear();
                for (const auto reg : visited) if (reg != root) slp_erased_results.insert(reg);
                break;
            }
        }
        if (!slp_erased_results.empty()) {
            for (auto& block : function.blocks) {
                auto& instructions = block.instructions;
                instructions.erase(std::remove_if(instructions.begin(), instructions.end(), [&](const Instruction& ins) {
                    return has_result(ins.opcode) && slp_erased_results.contains(ins.result);
                }), instructions.end());
            }
        }
    }

    // Vectorize canonical counted in-place integer loops. The scalar loop is
    // retained as an exact tail path; the added vector loop consumes one full
    // hardware vector per iteration and falls through to the original header
    // when fewer than vector_lanes elements remain. This keeps dynamic trip
    // counts correct without requiring masked tails on SSE2/AVX2.
    if (slp_cost_model.vector_integer_available && slp_cost_model.effective_vector_bits() >= 128U) {
        std::unordered_map<std::string, std::size_t> block_index;
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            block_index.emplace(function.blocks[index].name, index);
        std::unordered_map<std::string, std::vector<std::pair<std::size_t, std::size_t>>> incoming;
        for (std::size_t source_index = 0; source_index < function.blocks.size(); ++source_index) {
            for (std::size_t instruction_index = 0; instruction_index < function.blocks[source_index].instructions.size(); ++instruction_index) {
                const auto& instruction = function.blocks[source_index].instructions[instruction_index];
                for (const auto& successor : instruction.successors)
                    incoming[successor.block].push_back({source_index, instruction_index});
            }
        }

        bool vectorized_loop = false;
        for (std::size_t header_index = 0; header_index < function.blocks.size() && !vectorized_loop; ++header_index) {
            auto& header = function.blocks[header_index];
            if (header.parameters.size() != 2U || header.instructions.empty()) continue;
            auto& branch = header.instructions.back();
            if (branch.opcode != Opcode::branch_i1 || branch.successors.size() != 2U) continue;

            for (std::size_t body_successor_index = 0; body_successor_index < 2U && !vectorized_loop; ++body_successor_index) {
                const auto body_found = block_index.find(branch.successors[body_successor_index].block);
                if (body_found == block_index.end()) continue;
                const auto body_index = body_found->second;
                if (body_index == header_index) continue;
                auto& body = function.blocks[body_index];
                if (body.parameters.size() != 2U || body.instructions.size() < 4U) continue;
                const auto& backedge = body.instructions.back();
                if (backedge.opcode != Opcode::jump || backedge.successors.size() != 1U ||
                    backedge.successors.front().block != header.name || backedge.successors.front().arguments.size() != 2U) continue;

                ++stats.loop_vector_candidates_considered;
                const auto header_incoming = incoming.find(header.name);
                if (header_incoming == incoming.end() || header_incoming->second.size() != 2U) continue;
                std::optional<std::pair<std::size_t, std::size_t>> preheader_edge;
                for (const auto& edge : header_incoming->second)
                    if (edge.first != body_index) preheader_edge = edge;
                if (!preheader_edge) continue;

                const auto pointer_parameter = body.parameters[0];
                const auto count_parameter = body.parameters[1];
                Instruction* arithmetic = nullptr;
                Instruction* store = nullptr;
                Instruction* pointer_step = nullptr;
                Instruction* count_step = nullptr;
                std::size_t lane_bytes = 0U;
                VirtualRegister scalar = function.register_count;
                Opcode scalar_opcode{};

                for (auto& candidate : body.instructions) {
                    const bool i32_arithmetic = candidate.opcode == Opcode::add_i32 || candidate.opcode == Opcode::and_i32 ||
                                                candidate.opcode == Opcode::or_i32 || candidate.opcode == Opcode::xor_i32;
                    const bool i64_arithmetic = candidate.opcode == Opcode::add_i64 || candidate.opcode == Opcode::and_i64 ||
                                                candidate.opcode == Opcode::or_i64 || candidate.opcode == Opcode::xor_i64;
                    if ((i32_arithmetic || i64_arithmetic) && candidate.symbol == "$memptr" &&
                        candidate.inputs.size() == 2U && candidate.inputs[1] == pointer_parameter && candidate.immediate == 0) {
                        arithmetic = &candidate;
                        scalar = candidate.inputs[0];
                        scalar_opcode = candidate.opcode;
                        lane_bytes = i32_arithmetic ? 4U : 8U;
                    }
                }
                if (!arithmetic || scalar >= function.register_count) continue;
                bool scalar_loop_invariant = scalar != pointer_parameter && scalar != count_parameter;
                for (const auto& instruction : body.instructions)
                    if (has_result(instruction.opcode) && instruction.result == scalar) scalar_loop_invariant = false;
                for (const auto& instruction : header.instructions)
                    if (has_result(instruction.opcode) && instruction.result == scalar) scalar_loop_invariant = false;
                if (!scalar_loop_invariant) continue;
                if (branch.successors[body_successor_index].arguments.size() != 2U ||
                    branch.successors[body_successor_index].arguments[0] != header.parameters[0] ||
                    branch.successors[body_successor_index].arguments[1] != header.parameters[1]) continue;
                const auto store_opcode = lane_bytes == 4U ? Opcode::store_ptr_i32 : Opcode::store_ptr_i64;
                for (auto& candidate : body.instructions) {
                    if (candidate.opcode == store_opcode && candidate.inputs.size() == 2U &&
                        candidate.inputs[0] == arithmetic->result && candidate.inputs[1] == pointer_parameter && candidate.immediate == 0)
                        store = &candidate;
                    else if (candidate.opcode == Opcode::ptr_offset && candidate.inputs.size() == 1U &&
                             candidate.inputs[0] == pointer_parameter && candidate.immediate == static_cast<std::int64_t>(lane_bytes))
                        pointer_step = &candidate;
                    else if (candidate.opcode == Opcode::sub_i64 && candidate.inputs.size() == 1U &&
                             candidate.inputs[0] == count_parameter && candidate.symbol == "$imm" && candidate.immediate == 1)
                        count_step = &candidate;
                }
                if (!store || !pointer_step || !count_step || backedge.successors.front().arguments[0] != pointer_step->result ||
                    backedge.successors.front().arguments[1] != count_step->result) continue;

                const auto vector_bits = slp_cost_model.effective_vector_bits();
                const auto vector_lanes = static_cast<std::uint32_t>(vector_bits / (lane_bytes * 8U));
                if (vector_lanes < 2U || vector_lanes > 16U) { ++stats.loop_vector_candidates_rejected_target; continue; }
                const auto operation_multiplier = slp_operation_multiplier(scalar_opcode, slp_cost_model);
                const double scalar_iteration_cost = operation_multiplier * slp_cost_model.scalar_integer_throughput +
                    2.0 * slp_cost_model.scalar_memory_cost;
                const double vector_iteration_cost = operation_multiplier * slp_cost_model.vector_integer_throughput +
                    2.0 * slp_cost_model.vector_memory_cost * slp_cost_model.unaligned_memory_multiplier +
                    slp_cost_model.broadcast_cost + slp_cost_model.vector_setup_cost;
                if (!(vector_iteration_cost * slp_cost_model.minimum_speedup <
                      scalar_iteration_cost * static_cast<double>(vector_lanes))) {
                    ++stats.loop_vector_candidates_rejected_cost;
                    continue;
                }

                const auto allocate = [&](std::uint8_t width) {
                    const auto reg = function.register_count++;
                    function.register_widths.push_back(width);
                    function.register_classes.push_back(RegisterClass::integer);
                    return reg;
                };
                const auto vector_header_pointer = allocate(8U);
                const auto vector_header_count = allocate(8U);
                const auto vector_body_pointer = allocate(8U);
                const auto vector_body_count = allocate(8U);
                const auto vector_condition = allocate(1U);
                const auto vector_next_pointer = allocate(8U);
                const auto vector_next_count = allocate(8U);

                std::string vector_header_name = header.name + ".vector";
                std::string vector_body_name = header.name + ".vector.body";
                while (block_index.contains(vector_header_name)) vector_header_name += ".v";
                while (block_index.contains(vector_body_name) || vector_body_name == vector_header_name) vector_body_name += ".v";

                Block vector_body_block;
                vector_body_block.name = vector_body_name;
                vector_body_block.parameters = {vector_body_pointer, vector_body_count};
                Instruction packed;
                packed.opcode = lane_bytes == 4U ? Opcode::binary_i32_contiguous_inplace : Opcode::binary_i64_contiguous_inplace;
                packed.inputs = {vector_body_pointer, scalar};
                packed.immediate = static_cast<std::int64_t>(vector_lanes);
                packed.argument_index = static_cast<std::uint32_t>(scalar_opcode);
                packed.vector_bits = vector_bits;
                vector_body_block.instructions.push_back(std::move(packed));
                vector_body_block.instructions.push_back({Opcode::ptr_offset, vector_next_pointer, {vector_body_pointer},
                    static_cast<std::int64_t>(vector_lanes * lane_bytes), 0, {}, {}});
                Instruction vector_count_sub;
                vector_count_sub.opcode = Opcode::sub_i64;
                vector_count_sub.result = vector_next_count;
                vector_count_sub.inputs = {vector_body_count};
                vector_count_sub.immediate = static_cast<std::int64_t>(vector_lanes);
                vector_count_sub.symbol = "$imm";
                vector_body_block.instructions.push_back(std::move(vector_count_sub));
                Instruction vector_backedge;
                vector_backedge.opcode = Opcode::jump;
                vector_backedge.successors.push_back({vector_header_name, {vector_next_pointer, vector_next_count}});
                vector_body_block.instructions.push_back(std::move(vector_backedge));

                Block vector_header_block;
                vector_header_block.name = vector_header_name;
                vector_header_block.parameters = {vector_header_pointer, vector_header_count};
                Instruction compare;
                compare.opcode = Opcode::cmp_ge_i64;
                compare.result = vector_condition;
                compare.inputs = {vector_header_count};
                compare.immediate = static_cast<std::int64_t>(vector_lanes);
                compare.symbol = "$cmpimm";
                vector_header_block.instructions.push_back(std::move(compare));
                Instruction vector_branch;
                vector_branch.opcode = Opcode::branch_i1;
                vector_branch.inputs = {vector_condition};
                vector_branch.successors.push_back({vector_body_name, {vector_header_pointer, vector_header_count}});
                vector_branch.successors.push_back({header.name, {vector_header_pointer, vector_header_count}});
                vector_header_block.instructions.push_back(std::move(vector_branch));

                auto& preheader_instruction = function.blocks[preheader_edge->first].instructions[preheader_edge->second];
                for (auto& successor : preheader_instruction.successors) {
                    if (successor.block == header.name) {
                        successor.block = vector_header_name;
                        break;
                    }
                }

                const auto insert_at = std::min(body_index, header_index);
                function.blocks.insert(function.blocks.begin() + static_cast<std::ptrdiff_t>(insert_at), std::move(vector_body_block));
                function.blocks.insert(function.blocks.begin() + static_cast<std::ptrdiff_t>(insert_at + 1U), std::move(vector_header_block));
                ++stats.loop_vector_candidates_selected;
                if (vector_bits >= 512U) ++stats.loop_vector_width_512_selected;
                else if (vector_bits >= 256U) ++stats.loop_vector_width_256_selected;
                else ++stats.loop_vector_width_128_selected;
                vectorized_loop = true;
            }
        }
    }

    // Global machine dead-code elimination. Recompute full CFG liveness after
    // each sweep so chains spanning block boundaries collapse to a fixed point.
    // Potentially trapping operations and memory/call side effects are retained.
    bool removed_dead_code = true;
    while (removed_dead_code) {
        removed_dead_code = false;
        const auto liveness = analyze_liveness(function);
        stats.liveness_iterations += liveness.fixed_point_iterations;
        stats.cross_block_live_values = std::max(stats.cross_block_live_values,
                                                  liveness.cross_block_live_values);
        for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
            auto& instructions = function.blocks[block_index].instructions;
            std::vector<Instruction> retained_in_reverse;
            retained_in_reverse.reserve(instructions.size());
            for (std::size_t reverse = instructions.size(); reverse > 0; --reverse) {
                const auto instruction_index = reverse - 1U;
                auto& instruction = instructions[instruction_index];
                const bool result_is_dead = has_result(instruction.opcode) &&
                    instruction.result < function.register_count &&
                    !liveness.live_after[block_index][instruction_index][instruction.result];
                if (result_is_dead && is_removable_when_dead(instruction.opcode)) {
                    ++stats.dead_instructions_eliminated;
                    if (is_comparison(instruction.opcode)) ++stats.dead_comparisons_eliminated;
                    removed_dead_code = true;
                    continue;
                }
                retained_in_reverse.push_back(std::move(instruction));
            }
            std::reverse(retained_in_reverse.begin(), retained_in_reverse.end());
            instructions = std::move(retained_in_reverse);
        }
    }

    stats.instructions_after = 0;
    for (const auto& block : function.blocks)
        stats.instructions_after += static_cast<std::uint32_t>(block.instructions.size());

    // Remove virtual-register holes left by erased alias definitions. Keeping
    // the machine IR dense preserves the verifier invariant that every virtual
    // register has exactly one definition and avoids allocating dead aliases.
    std::vector<bool> retained(function.register_count, false);
    for (const auto& block : function.blocks) {
        for (const auto parameter : block.parameters) if (parameter < retained.size()) retained[parameter] = true;
        for (const auto& instruction : block.instructions) {
            if (instruction.result < retained.size()) {
                const bool has_result = instruction.opcode != Opcode::jump && instruction.opcode != Opcode::branch_i1 &&
                    instruction.opcode != Opcode::return_i32 && instruction.opcode != Opcode::return_i64 &&
                    instruction.opcode != Opcode::return_f32 && instruction.opcode != Opcode::return_f64 &&
                    instruction.opcode != Opcode::return_void && instruction.opcode != Opcode::return_aggregate && instruction.opcode != Opcode::call_void && instruction.opcode != Opcode::call_aggregate &&
                    instruction.opcode != Opcode::call_indirect_void &&
                    instruction.opcode != Opcode::store_stack_i8 && instruction.opcode != Opcode::store_stack_i16 &&
                    instruction.opcode != Opcode::store_stack_i32 && instruction.opcode != Opcode::store_stack_i64 &&
                    instruction.opcode != Opcode::store_stack_f32 && instruction.opcode != Opcode::store_stack_f64 &&
                    instruction.opcode != Opcode::store_ptr_i8 && instruction.opcode != Opcode::store_ptr_i16 &&
                    instruction.opcode != Opcode::store_ptr_i32 && instruction.opcode != Opcode::store_ptr_i64 &&
                    instruction.opcode != Opcode::store_ptr_f32 && instruction.opcode != Opcode::store_ptr_f64;
                if (has_result) retained[instruction.result] = true;
            }
        }
    }
    std::vector<VirtualRegister> remap(function.register_count, function.register_count);
    std::vector<std::uint8_t> widths;
    std::vector<RegisterClass> classes;
    widths.reserve(function.register_count);
    classes.reserve(function.register_count);
    for (VirtualRegister reg = 0; reg < function.register_count; ++reg) {
        if (!retained[reg]) continue;
        remap[reg] = static_cast<VirtualRegister>(widths.size());
        widths.push_back(function.register_widths[reg]);
        classes.push_back(function.register_classes[reg]);
    }
    for (auto& block : function.blocks) {
        for (auto& parameter : block.parameters) parameter = remap[parameter];
        for (auto& instruction : block.instructions) {
            for (auto& input : instruction.inputs) input = remap[input];
            for (auto& successor : instruction.successors)
                for (auto& argument : successor.arguments) argument = remap[argument];
            if (instruction.result < remap.size() && remap[instruction.result] != function.register_count)
                instruction.result = remap[instruction.result];
        }
    }
    function.register_count = static_cast<VirtualRegister>(widths.size());
    function.register_widths = std::move(widths);
    function.register_classes = std::move(classes);

    function.machine_instructions_before_optimization = stats.instructions_before;
    function.machine_copies_propagated = stats.copies_propagated;
    function.machine_zero_offsets_eliminated = stats.zero_offsets_eliminated;
    function.machine_redundant_casts_eliminated = stats.redundant_casts_eliminated;
    function.machine_address_modes_folded = stats.address_modes_folded;
    function.machine_compare_branches_fused = stats.compare_branches_fused;
    function.machine_compare_branch_bytes_avoided = stats.compare_branch_bytes_avoided;
    function.machine_floating_compare_branches_fused = stats.floating_compare_branches_fused;
    function.machine_floating_compare_branch_bytes_avoided = stats.floating_compare_branch_bytes_avoided;
    function.machine_jump_threads = stats.jump_threads;
    function.machine_empty_blocks_removed = stats.empty_blocks_removed;
    function.machine_unreachable_blocks_removed = stats.unreachable_blocks_removed;
    function.machine_blocks_reordered = stats.blocks_reordered;
    function.machine_immediate_forms_selected = stats.immediate_forms_selected;
    function.machine_constant_definitions_eliminated = stats.constant_definitions_eliminated;
    function.machine_immediate_comparisons_selected = stats.immediate_comparisons_selected;
    function.machine_direct_constant_returns = stats.direct_constant_returns;
    function.machine_zeroing_idioms_selected = stats.zeroing_idioms_selected;
    function.machine_constant_stores_selected = stats.constant_stores_selected;
    function.machine_extension_chains_eliminated = stats.extension_chains_eliminated;
    function.machine_load_returns_folded = stats.load_returns_folded;
    function.machine_load_arithmetic_folded = stats.load_arithmetic_folded;
    function.machine_dead_instructions_eliminated = stats.dead_instructions_eliminated;
    function.machine_dead_comparisons_eliminated = stats.dead_comparisons_eliminated;
    function.machine_cross_block_copies_propagated = stats.cross_block_copies_propagated;
    function.machine_liveness_iterations = stats.liveness_iterations;
    function.machine_cross_block_live_values = stats.cross_block_live_values;
    return stats;
}

OptimizationStats optimize_module(Module& module, const SlpCostModel& slp_cost_model) {
    OptimizationStats total;
    for (auto& function : module.functions) {
        const auto stats = optimize_function(function, slp_cost_model);
        total.instructions_before += stats.instructions_before;
        total.instructions_after += stats.instructions_after;
        total.copies_propagated += stats.copies_propagated;
        total.zero_offsets_eliminated += stats.zero_offsets_eliminated;
        total.redundant_casts_eliminated += stats.redundant_casts_eliminated;
        total.address_modes_folded += stats.address_modes_folded;
        total.compare_branches_fused += stats.compare_branches_fused;
        total.compare_branch_bytes_avoided += stats.compare_branch_bytes_avoided;
        total.floating_compare_branches_fused += stats.floating_compare_branches_fused;
        total.floating_compare_branch_bytes_avoided += stats.floating_compare_branch_bytes_avoided;
        total.jump_threads += stats.jump_threads;
        total.empty_blocks_removed += stats.empty_blocks_removed;
        total.unreachable_blocks_removed += stats.unreachable_blocks_removed;
        total.blocks_reordered += stats.blocks_reordered;
        total.immediate_forms_selected += stats.immediate_forms_selected;
        total.constant_definitions_eliminated += stats.constant_definitions_eliminated;
        total.immediate_comparisons_selected += stats.immediate_comparisons_selected;
        total.direct_constant_returns += stats.direct_constant_returns;
        total.zeroing_idioms_selected += stats.zeroing_idioms_selected;
        total.constant_stores_selected += stats.constant_stores_selected;
        total.extension_chains_eliminated += stats.extension_chains_eliminated;
        total.load_returns_folded += stats.load_returns_folded;
        total.load_arithmetic_folded += stats.load_arithmetic_folded;
        total.dead_instructions_eliminated += stats.dead_instructions_eliminated;
        total.dead_comparisons_eliminated += stats.dead_comparisons_eliminated;
        total.cross_block_copies_propagated += stats.cross_block_copies_propagated;
        total.liveness_iterations += stats.liveness_iterations;
        total.cross_block_live_values += stats.cross_block_live_values;
        total.slp_candidates_considered += stats.slp_candidates_considered;
        total.slp_candidates_selected += stats.slp_candidates_selected;
        total.slp_candidates_rejected_cost += stats.slp_candidates_rejected_cost;
        total.slp_candidates_rejected_target += stats.slp_candidates_rejected_target;
        total.slp_width_128_selected += stats.slp_width_128_selected;
        total.slp_width_256_selected += stats.slp_width_256_selected;
        total.slp_width_512_selected += stats.slp_width_512_selected;
        total.loop_vector_candidates_considered += stats.loop_vector_candidates_considered;
        total.loop_vector_candidates_selected += stats.loop_vector_candidates_selected;
        total.loop_vector_candidates_rejected_target += stats.loop_vector_candidates_rejected_target;
        total.loop_vector_candidates_rejected_cost += stats.loop_vector_candidates_rejected_cost;
        total.loop_vector_width_128_selected += stats.loop_vector_width_128_selected;
        total.loop_vector_width_256_selected += stats.loop_vector_width_256_selected;
        total.loop_vector_width_512_selected += stats.loop_vector_width_512_selected;
        total.slp_estimated_scalar_cost += stats.slp_estimated_scalar_cost;
        total.slp_estimated_vector_cost += stats.slp_estimated_vector_cost;
        total.slp_estimated_memory_cost += stats.slp_estimated_memory_cost;
        total.slp_estimated_shuffle_cost += stats.slp_estimated_shuffle_cost;
        total.slp_estimated_register_pressure_cost += stats.slp_estimated_register_pressure_cost;
    }
    return total;
}

} // namespace forge::machine
