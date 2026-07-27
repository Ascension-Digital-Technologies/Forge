// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <optional>
#include <limits>
#include <span>
#include <vector>
#include <string_view>

#include "forge/ir/module.hpp"

namespace forge::target {

enum class Endianness { little, big };

struct StructFieldLayout {
    ir::Type type;
    std::size_t offset{};
    std::size_t size{};
    std::size_t alignment{};
};

struct ArrayLayout {
    ir::Type element_type;
    std::size_t element_count{};
    std::size_t stride{};
    std::size_t size{};
    std::size_t alignment{1};
};

struct StructLayout {
    std::vector<StructFieldLayout> fields;
    std::size_t size{};
    std::size_t alignment{1};
};

struct DataLayout {
    std::size_t pointer_size{8};
    std::size_t pointer_alignment{8};
    Endianness endianness{Endianness::little};

    [[nodiscard]] static DataLayout host() noexcept;
    [[nodiscard]] std::optional<std::size_t> size_of(ir::Type type) const noexcept;
    [[nodiscard]] std::optional<std::size_t> alignment_of(ir::Type type) const noexcept;
    [[nodiscard]] std::optional<std::size_t> array_size(ir::Type element, std::size_t count) const noexcept;
    [[nodiscard]] std::optional<std::size_t> array_stride(ir::Type element) const noexcept;
    [[nodiscard]] std::optional<ArrayLayout> array_layout(const ir::ArrayDecl& declaration) const noexcept;
    [[nodiscard]] std::optional<StructLayout> struct_layout(std::span<const ir::Type> fields) const noexcept;
    [[nodiscard]] std::optional<StructLayout> struct_layout(const ir::StructDecl& declaration) const noexcept;
    [[nodiscard]] std::optional<StructLayout> struct_layout(const ir::Module& module, const ir::StructDecl& declaration) const noexcept;
    [[nodiscard]] std::optional<std::size_t> aggregate_size(const ir::Module& module, ir::AggregateRefKind kind, std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> aggregate_alignment(const ir::Module& module, ir::AggregateRefKind kind, std::string_view name) const noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
};

[[nodiscard]] constexpr bool is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

[[nodiscard]] constexpr bool is_aligned(std::size_t value, std::size_t alignment) noexcept {
    return is_power_of_two(alignment) && (value & (alignment - 1)) == 0;
}

[[nodiscard]] constexpr std::optional<std::size_t> checked_align_to(
    std::size_t value, std::size_t alignment) noexcept {
    if (!is_power_of_two(alignment)) return std::nullopt;
    const auto mask = alignment - 1;
    if (value > std::numeric_limits<std::size_t>::max() - mask) return std::nullopt;
    return (value + mask) & ~mask;
}

[[nodiscard]] constexpr std::size_t align_to(std::size_t value, std::size_t alignment) noexcept {
    const auto aligned = checked_align_to(value, alignment);
    return aligned.value_or(value);
}

} // namespace forge::target
