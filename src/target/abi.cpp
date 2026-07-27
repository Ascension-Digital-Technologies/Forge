// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include "forge/target/abi.hpp"

#include <algorithm>

namespace forge::target {
namespace {

AbiValueClass merge_class(AbiValueClass left, AbiValueClass right) noexcept {
    if (left == AbiValueClass::none) return right;
    if (right == AbiValueClass::none) return left;
    if (left == AbiValueClass::memory || right == AbiValueClass::memory) return AbiValueClass::memory;
    if (left == AbiValueClass::integer || right == AbiValueClass::integer) return AbiValueClass::integer;
    return AbiValueClass::sse;
}

bool mark_scalar(AggregateAbiClassification& result, ir::Type type,
                 std::size_t offset, std::size_t size) noexcept {
    if (size == 0 || offset > result.size || size > result.size - offset) return false;
    const auto first = offset / 8U;
    const auto last = (offset + size - 1U) / 8U;
    if (last >= result.classes.size()) return false;
    const auto value_class = type.is_float() ? AbiValueClass::sse : AbiValueClass::integer;
    for (std::size_t index = first; index <= last; ++index)
        result.classes[index] = merge_class(result.classes[index], value_class);
    return true;
}

bool classify_structure_fields(const ir::Module& module, const ir::StructDecl& declaration,
                               const DataLayout& layout, AggregateAbiClassification& result,
                               std::size_t base_offset) noexcept;

bool classify_nested(const ir::Module& module, ir::AggregateRefKind kind, std::string_view name,
                     const DataLayout& layout, AggregateAbiClassification& result,
                     std::size_t base_offset) noexcept {
    if (kind == ir::AggregateRefKind::structure) {
        const auto found = std::find_if(module.structs().begin(), module.structs().end(),
            [&](const ir::StructDecl& item) { return item.name == name; });
        return found != module.structs().end() &&
               classify_structure_fields(module, *found, layout, result, base_offset);
    }
    if (kind == ir::AggregateRefKind::array) {
        const auto found = std::find_if(module.arrays().begin(), module.arrays().end(),
            [&](const ir::ArrayDecl& item) { return item.name == name; });
        if (found == module.arrays().end()) return false;
        const auto array = layout.array_layout(*found);
        if (!array) return false;
        for (std::size_t index = 0; index < array->element_count; ++index)
            if (!mark_scalar(result, array->element_type, base_offset + index * array->stride,
                             *layout.size_of(array->element_type))) return false;
        return true;
    }
    return false;
}

bool classify_structure_fields(const ir::Module& module, const ir::StructDecl& declaration,
                               const DataLayout& layout, AggregateAbiClassification& result,
                               std::size_t base_offset) noexcept {
    const auto structure = layout.struct_layout(module, declaration);
    if (!structure || structure->fields.size() != declaration.fields.size()) return false;
    for (std::size_t index = 0; index < declaration.fields.size(); ++index) {
        const auto& field = declaration.fields[index];
        const auto& field_layout = structure->fields[index];
        if (field_layout.alignment != 0 && ((base_offset + field_layout.offset) % field_layout.alignment) != 0)
            return false;
        if (field.aggregate_kind == ir::AggregateRefKind::scalar) {
            if (!mark_scalar(result, field.type, base_offset + field_layout.offset, field_layout.size)) return false;
        } else if (!classify_nested(module, field.aggregate_kind, field.aggregate_name, layout,
                                    result, base_offset + field_layout.offset)) {
            return false;
        }
    }
    return true;
}

AggregateAbiClassification scalar_classification(ir::Type type, NativeAbi abi,
                                                  const DataLayout& layout) noexcept {
    AggregateAbiClassification result;
    const auto size = layout.size_of(type);
    const auto alignment = layout.alignment_of(type);
    if (!size || !alignment) return result;
    result.size = *size;
    result.alignment = *alignment;
    result.classes[0] = type.is_float() ? AbiValueClass::sse : AbiValueClass::integer;
    result.register_count = 1;
    result.passed_indirectly = false;
    result.returned_indirectly = false;
    if (abi == NativeAbi::windows_x64 && type.kind() == ir::TypeKind::void_) result.register_count = 0;
    return result;
}

} // namespace

std::optional<AggregateAbiClassification> classify_aggregate(
    const ir::Module& module, ir::AggregateRefKind kind, std::string_view name,
    NativeAbi abi, const DataLayout& layout) noexcept {
    const auto size = layout.aggregate_size(module, kind, name);
    const auto alignment = layout.aggregate_alignment(module, kind, name);
    if (!size || !alignment || *size == 0) return std::nullopt;

    AggregateAbiClassification result;
    result.size = *size;
    result.alignment = *alignment;

    if (abi == NativeAbi::windows_x64) {
        const bool direct = *size == 1 || *size == 2 || *size == 4 || *size == 8;
        result.classes[0] = direct ? AbiValueClass::integer : AbiValueClass::indirect;
        result.register_count = direct ? 1U : 0U;
        result.passed_indirectly = !direct;
        result.returned_indirectly = !direct;
        return result;
    }

    if (*size > 16 || *alignment > 16) {
        result.classes[0] = AbiValueClass::memory;
        result.passed_indirectly = true;
        result.returned_indirectly = true;
        return result;
    }

    bool ok = classify_nested(module, kind, name, layout, result, 0);
    if (!ok || result.classes[0] == AbiValueClass::memory || result.classes[1] == AbiValueClass::memory) {
        result.classes = {AbiValueClass::memory, AbiValueClass::none};
        result.passed_indirectly = true;
        result.returned_indirectly = true;
        return result;
    }
    result.register_count = static_cast<std::uint8_t>((*size + 7U) / 8U);
    for (std::size_t index = 0; index < result.register_count; ++index)
        if (result.classes[index] == AbiValueClass::none) result.classes[index] = AbiValueClass::integer;
    return result;
}

FunctionAbiClassification classify_function(const ir::Module& module, const ir::Function& function,
                                             NativeAbi abi, const DataLayout& layout) noexcept {
    FunctionAbiClassification result;
    result.variadic = function.variadic;
    if (function.returns_aggregate()) {
        result.result = classify_aggregate(module, function.return_aggregate_kind,
                                           function.return_aggregate_name, abi, layout).value_or(AggregateAbiClassification{});
    } else if (function.return_type.kind() != ir::TypeKind::void_) {
        result.result = scalar_classification(function.return_type, abi, layout);
    }

    std::size_t integer_limit = abi == NativeAbi::system_v_x86_64 ? 6U : 4U;
    std::size_t floating_limit = abi == NativeAbi::system_v_x86_64 ? 8U : 4U;
    std::size_t positional = 0;
    for (const auto& parameter : function.parameters) {
        AggregateAbiClassification classification;
        if (parameter.is_aggregate()) {
            classification = classify_aggregate(module, parameter.aggregate_kind,
                                                parameter.aggregate_name, abi, layout).value_or(AggregateAbiClassification{});
        } else {
            classification = scalar_classification(parameter.type, abi, layout);
        }
        result.parameters.push_back(classification);
        if (!classification.valid()) continue;
        if (classification.passed_indirectly) classification.classes[0] = AbiValueClass::integer;

        if (abi == NativeAbi::windows_x64) {
            if (positional < 4U) {
                if (classification.classes[0] == AbiValueClass::sse) ++result.floating_registers;
                else ++result.integer_registers;
            } else {
                result.stack_bytes += 8U;
            }
            ++positional;
            continue;
        }

        for (std::size_t index = 0; index < std::max<std::size_t>(1, classification.register_count); ++index) {
            const auto value_class = classification.classes[index];
            if (value_class == AbiValueClass::sse && result.floating_registers < floating_limit) {
                ++result.floating_registers;
            } else if (value_class != AbiValueClass::sse && result.integer_registers < integer_limit) {
                ++result.integer_registers;
            } else {
                result.stack_bytes += 8U;
            }
        }
    }
    result.stack_bytes = align_to(result.stack_bytes, 8U);
    return result;
}

const char* abi_value_class_name(AbiValueClass value) noexcept {
    switch (value) {
    case AbiValueClass::none: return "none";
    case AbiValueClass::integer: return "integer";
    case AbiValueClass::sse: return "sse";
    case AbiValueClass::memory: return "memory";
    case AbiValueClass::indirect: return "indirect";
    }
    return "unknown";
}

} // namespace forge::target
