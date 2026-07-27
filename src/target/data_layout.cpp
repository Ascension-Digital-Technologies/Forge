#include "forge/target/data_layout.hpp"

#include <bit>
#include <algorithm>
#include <limits>
#include <string>

namespace forge::target {

DataLayout DataLayout::host() noexcept {
    DataLayout layout;
    layout.pointer_size = sizeof(void*);
    layout.pointer_alignment = alignof(void*);
    layout.endianness = std::endian::native == std::endian::big ? Endianness::big : Endianness::little;
    return layout;
}

std::optional<std::size_t> DataLayout::size_of(ir::Type type) const noexcept {
    switch (type.kind()) {
    case ir::TypeKind::i1:
    case ir::TypeKind::i8: return 1;
    case ir::TypeKind::i16: return 2;
    case ir::TypeKind::i32:
    case ir::TypeKind::f32: return 4;
    case ir::TypeKind::i64:
    case ir::TypeKind::f64: return 8;
    case ir::TypeKind::ptr: return pointer_size;
    case ir::TypeKind::void_: return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::size_t> DataLayout::alignment_of(ir::Type type) const noexcept {
    if (type.kind() == ir::TypeKind::ptr) return pointer_alignment;
    return size_of(type);
}

std::optional<std::size_t> DataLayout::array_stride(ir::Type element) const noexcept {
    const auto size = size_of(element);
    const auto alignment = alignment_of(element);
    if (!size || !alignment) return std::nullopt;
    return checked_align_to(*size, *alignment);
}

std::optional<std::size_t> DataLayout::array_size(ir::Type element, std::size_t count) const noexcept {
    const auto stride = array_stride(element);
    if (!stride) return std::nullopt;
    if (count != 0 && *stride > std::numeric_limits<std::size_t>::max() / count) return std::nullopt;
    return *stride * count;
}


std::optional<ArrayLayout> DataLayout::array_layout(const ir::ArrayDecl& declaration) const noexcept {
    if (declaration.element_count == 0) return std::nullopt;
    const auto stride = array_stride(declaration.element_type);
    const auto size = array_size(declaration.element_type, declaration.element_count);
    const auto alignment = alignment_of(declaration.element_type);
    if (!stride || !size || !alignment) return std::nullopt;
    return ArrayLayout{declaration.element_type, declaration.element_count, *stride, *size, *alignment};
}

std::optional<StructLayout> DataLayout::struct_layout(std::span<const ir::Type> field_types) const noexcept {
    StructLayout result;
    result.fields.reserve(field_types.size());
    std::size_t cursor = 0;
    for (const auto type : field_types) {
        const auto size = size_of(type);
        const auto alignment = alignment_of(type);
        if (!size || !alignment || !is_power_of_two(*alignment)) return std::nullopt;
        const auto offset = checked_align_to(cursor, *alignment);
        if (!offset || *size > std::numeric_limits<std::size_t>::max() - *offset) return std::nullopt;
        result.fields.push_back({type, *offset, *size, *alignment});
        cursor = *offset + *size;
        if (*alignment > result.alignment) result.alignment = *alignment;
    }
    const auto total = checked_align_to(cursor, result.alignment);
    if (!total) return std::nullopt;
    result.size = *total;
    return result;
}

bool DataLayout::is_valid() const noexcept {
    return (pointer_size == 4 || pointer_size == 8) && pointer_alignment != 0 &&
           is_power_of_two(pointer_alignment);
}

} // namespace forge::target

std::optional<forge::target::StructLayout> forge::target::DataLayout::struct_layout(const ir::StructDecl& declaration) const noexcept {
    std::vector<ir::Type> fields;
    fields.reserve(declaration.fields.size());
    for (const auto& field : declaration.fields) fields.push_back(field.type);
    return struct_layout(fields);
}

namespace {
std::optional<forge::target::StructLayout> layout_struct_recursive(
    const forge::target::DataLayout& layout,
    const forge::ir::Module& module,
    const forge::ir::StructDecl& declaration,
    std::vector<std::string>& stack) noexcept {
    if (std::find(stack.begin(), stack.end(), declaration.name) != stack.end()) return std::nullopt;
    stack.push_back(declaration.name);
    forge::target::StructLayout result;
    result.fields.reserve(declaration.fields.size());
    std::size_t cursor = 0;
    for (const auto& field : declaration.fields) {
        std::optional<std::size_t> size;
        std::optional<std::size_t> alignment;
        if (field.aggregate_kind == forge::ir::AggregateRefKind::scalar) {
            size = layout.size_of(field.type);
            alignment = layout.alignment_of(field.type);
        } else if (field.aggregate_kind == forge::ir::AggregateRefKind::array) {
            const auto found = std::find_if(module.arrays().begin(), module.arrays().end(), [&](const forge::ir::ArrayDecl& item) {
                return item.name == field.aggregate_name;
            });
            if (found != module.arrays().end()) {
                const auto nested = layout.array_layout(*found);
                if (nested) { size = nested->size; alignment = nested->alignment; }
            }
        } else {
            const auto found = std::find_if(module.structs().begin(), module.structs().end(), [&](const forge::ir::StructDecl& item) {
                return item.name == field.aggregate_name;
            });
            if (found != module.structs().end()) {
                const auto nested = layout_struct_recursive(layout, module, *found, stack);
                if (nested) { size = nested->size; alignment = nested->alignment; }
            }
        }
        if (!size || !alignment || !forge::target::is_power_of_two(*alignment)) { stack.pop_back(); return std::nullopt; }
        const auto offset = forge::target::checked_align_to(cursor, *alignment);
        if (!offset || *size > std::numeric_limits<std::size_t>::max() - *offset) { stack.pop_back(); return std::nullopt; }
        result.fields.push_back({field.type, *offset, *size, *alignment});
        cursor = *offset + *size;
        result.alignment = std::max(result.alignment, *alignment);
    }
    const auto total = forge::target::checked_align_to(cursor, result.alignment);
    stack.pop_back();
    if (!total) return std::nullopt;
    result.size = *total;
    return result;
}
}

std::optional<forge::target::StructLayout> forge::target::DataLayout::struct_layout(
    const ir::Module& module, const ir::StructDecl& declaration) const noexcept {
    std::vector<std::string> stack;
    return layout_struct_recursive(*this, module, declaration, stack);
}

std::optional<std::size_t> forge::target::DataLayout::aggregate_size(
    const ir::Module& module, ir::AggregateRefKind kind, std::string_view name) const noexcept {
    if (kind == ir::AggregateRefKind::array) {
        const auto found = std::find_if(module.arrays().begin(), module.arrays().end(), [&](const ir::ArrayDecl& item) { return item.name == name; });
        if (found == module.arrays().end()) return std::nullopt;
        const auto layout = array_layout(*found);
        return layout ? std::optional<std::size_t>(layout->size) : std::nullopt;
    }
    if (kind == ir::AggregateRefKind::structure) {
        const auto found = std::find_if(module.structs().begin(), module.structs().end(), [&](const ir::StructDecl& item) { return item.name == name; });
        if (found == module.structs().end()) return std::nullopt;
        const auto layout = struct_layout(module, *found);
        return layout ? std::optional<std::size_t>(layout->size) : std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::size_t> forge::target::DataLayout::aggregate_alignment(
    const ir::Module& module, ir::AggregateRefKind kind, std::string_view name) const noexcept {
    if (kind == ir::AggregateRefKind::array) {
        const auto found = std::find_if(module.arrays().begin(), module.arrays().end(), [&](const ir::ArrayDecl& item) { return item.name == name; });
        if (found == module.arrays().end()) return std::nullopt;
        const auto layout = array_layout(*found);
        return layout ? std::optional<std::size_t>(layout->alignment) : std::nullopt;
    }
    if (kind == ir::AggregateRefKind::structure) {
        const auto found = std::find_if(module.structs().begin(), module.structs().end(), [&](const ir::StructDecl& item) { return item.name == name; });
        if (found == module.structs().end()) return std::nullopt;
        const auto layout = struct_layout(module, *found);
        return layout ? std::optional<std::size_t>(layout->alignment) : std::nullopt;
    }
    return std::nullopt;
}
