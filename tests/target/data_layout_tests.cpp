#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
#include <stdexcept>

#include "forge/interpreter/interpreter.hpp"
#include "forge/target/data_layout.hpp"

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    try {
        const auto layout = forge::target::DataLayout::host();
        require(layout.is_valid(), "host data layout is invalid");
        require(forge::target::is_power_of_two(layout.pointer_alignment), "pointer alignment is not a power of two");
        require(forge::target::is_aligned(16, 8), "aligned value was rejected");
        require(!forge::target::is_aligned(18, 8), "misaligned value was accepted");
        require(!forge::target::is_aligned(16, 3), "non-power-of-two alignment was accepted");
        require(forge::target::checked_align_to(17, 8) == 24, "checked alignment returned the wrong value");
        require(!forge::target::checked_align_to(17, 3).has_value(), "invalid alignment was accepted");
        require(!forge::target::checked_align_to(std::numeric_limits<std::size_t>::max(), 8).has_value(), "alignment overflow was accepted");
        require(layout.pointer_size == sizeof(void*), "host pointer size mismatch");
        require(layout.pointer_alignment == alignof(void*), "host pointer alignment mismatch");
        require(layout.size_of(forge::ir::Type(forge::ir::TypeKind::i1)) == 1, "i1 storage size mismatch");
        require(layout.size_of(forge::ir::Type(forge::ir::TypeKind::i16)) == 2, "i16 size mismatch");
        require(layout.size_of(forge::ir::Type(forge::ir::TypeKind::i64)) == 8, "i64 size mismatch");
        require(layout.size_of(forge::ir::Type(forge::ir::TypeKind::ptr)) == sizeof(void*), "ptr size mismatch");
        require(!layout.size_of(forge::ir::Type(forge::ir::TypeKind::void_)).has_value(), "void unexpectedly has a size");
        require(forge::target::align_to(9, 8) == 16, "alignment rounding failed");
        require(layout.array_stride(forge::ir::Type(forge::ir::TypeKind::i64)) == 8, "i64 array stride mismatch");
        require(layout.array_size(forge::ir::Type(forge::ir::TypeKind::i16), 7) == 14, "i16 array size mismatch");
        require(layout.array_size(forge::ir::Type(forge::ir::TypeKind::i8), 0) == 0, "empty array size mismatch");
        require(!layout.array_size(forge::ir::Type(forge::ir::TypeKind::void_), 1).has_value(), "void array unexpectedly has a layout");
        require(!layout.array_size(forge::ir::Type(forge::ir::TypeKind::i64), std::numeric_limits<std::size_t>::max()).has_value(), "array size overflow was accepted");
        forge::ir::ArrayDecl words{"Words", forge::ir::Type(forge::ir::TypeKind::i16), 8};
        const auto words_layout = layout.array_layout(words);
        require(words_layout.has_value(), "named array layout was rejected");
        require(words_layout->stride == 2 && words_layout->size == 16 && words_layout->alignment == 2,
                "named array layout mismatch");
        forge::ir::ArrayDecl invalid_words{"Invalid", forge::ir::Type(forge::ir::TypeKind::void_), 4};
        require(!layout.array_layout(invalid_words).has_value(), "unsized named array was accepted");
        forge::ir::ArrayDecl empty_words{"Empty", forge::ir::Type(forge::ir::TypeKind::i8), 0};
        require(!layout.array_layout(empty_words).has_value(), "zero-length named array was accepted");

        const std::array struct_fields{
            forge::ir::Type(forge::ir::TypeKind::i8),
            forge::ir::Type(forge::ir::TypeKind::i64),
            forge::ir::Type(forge::ir::TypeKind::i16)};
        const auto structure = layout.struct_layout(struct_fields);
        require(structure.has_value(), "valid structure layout was rejected");
        require(structure->fields.size() == 3, "structure field count mismatch");
        require(structure->fields[0].offset == 0, "first structure field offset mismatch");
        require(structure->fields[1].offset == 8, "aligned i64 field offset mismatch");
        require(structure->fields[2].offset == 16, "trailing i16 field offset mismatch");
        require(structure->alignment == 8, "structure alignment mismatch");
        require(structure->size == 24, "structure tail padding mismatch");
        forge::ir::StructDecl named_structure{"Record", {{"tag", forge::ir::Type(forge::ir::TypeKind::i8)},
                                                           {"first", forge::ir::Type(forge::ir::TypeKind::i64)},
                                                           {"second", forge::ir::Type(forge::ir::TypeKind::i16)}}};
        const auto named_layout = layout.struct_layout(named_structure);
        require(named_layout.has_value() && named_layout->fields[1].offset == 8 && named_layout->size == 24,
                "named structure layout mismatch");
        forge::ir::Module aggregate_module("nested");
        aggregate_module.arrays().push_back(words);
        forge::ir::StructDecl inner{"Inner", {{"value", forge::ir::Type(forge::ir::TypeKind::i64)},
                                                   {"words", forge::ir::Type(forge::ir::TypeKind::void_), forge::ir::AggregateRefKind::array, "Words"}}};
        forge::ir::StructDecl outer{"Outer", {{"tag", forge::ir::Type(forge::ir::TypeKind::i8)},
                                                   {"inner", forge::ir::Type(forge::ir::TypeKind::void_), forge::ir::AggregateRefKind::structure, "Inner"},
                                                   {"tail", forge::ir::Type(forge::ir::TypeKind::i64)}}};
        aggregate_module.structs().push_back(inner);
        aggregate_module.structs().push_back(outer);
        const auto inner_layout = layout.struct_layout(aggregate_module, aggregate_module.structs()[0]);
        const auto outer_layout = layout.struct_layout(aggregate_module, aggregate_module.structs()[1]);
        require(inner_layout.has_value() && inner_layout->size == 24 && inner_layout->fields[1].offset == 8,
                "nested array field layout mismatch");
        require(outer_layout.has_value() && outer_layout->size == 40 && outer_layout->fields[1].offset == 8 && outer_layout->fields[2].offset == 32,
                "nested structure field layout mismatch");
        forge::ir::Module recursive_module("recursive");
        recursive_module.structs().push_back({"Cycle", {{"self", forge::ir::Type(forge::ir::TypeKind::void_), forge::ir::AggregateRefKind::structure, "Cycle"}}});
        require(!layout.struct_layout(recursive_module, recursive_module.structs()[0]).has_value(),
                "recursive structure layout was accepted");

        const std::array invalid_struct{forge::ir::Type(forge::ir::TypeKind::void_)};
        require(!layout.struct_layout(invalid_struct).has_value(), "void structure field unexpectedly has a layout");
        const std::array<forge::ir::Type, 0> empty_struct{};
        const auto empty_layout = layout.struct_layout(empty_struct);
        require(empty_layout.has_value() && empty_layout->size == 0 && empty_layout->alignment == 1,
                "empty structure layout mismatch");

        std::array<std::uint8_t, 16> storage{};
        auto pointer = forge::interpreter::Value::host_pointer(storage.data(), storage.size());
        require(pointer.kind() == forge::interpreter::Value::Kind::pointer, "host pointer kind mismatch");
        require(pointer.host_address() == storage.data(), "host pointer address mismatch");
        require(pointer.as_pointer().object->size() == storage.size(), "host pointer extent mismatch");

        std::cout << "Forge target data-layout tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
