#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include "forge/ir/parser.hpp"
#include "forge/ir/verifier.hpp"
#include "forge/machine/lower.hpp"
#include "forge/object/elf.hpp"

namespace {
std::uint16_t u16(const std::vector<std::byte>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes.at(offset))) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes.at(offset + 1))) << 8U;
}
std::uint64_t u64(const std::vector<std::byte>& bytes, std::size_t offset) {
    std::uint64_t value{};
    for (unsigned shift = 0; shift < 64; shift += 8)
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes.at(offset + shift / 8U))) << shift;
    return value;
}
}

int main() {
    const std::string source = R"(
module @object_test {
  global @counter: i64 = 7
  extern func @host_add(%left: i64, %right: i64) -> i64
  func @entry(%value: i64) -> i64 {
  entry:
    %counter = global.address ptr @counter
    %loaded = load i64 %counter align 8
    %result = call i64 @host_add(%loaded, %value)
    return %result
  }
}
)";
    auto parsed = forge::ir::parse_module(source);
    if (!parsed.ok()) return 1;
    if (!forge::ir::verify_module(*parsed.module).empty()) return 2;
    auto lowered = forge::machine::lower_module(*parsed.module);
    if (!lowered.ok()) return 3;
    auto object = forge::object::emit_elf64_x86_64(*lowered.module);
    if (!object.ok()) return 4;
    if (object.bytes.size() < 64) return 5;
    if (object.bytes[0] != std::byte{0x7f} || object.bytes[1] != std::byte{'E'} ||
        object.bytes[2] != std::byte{'L'} || object.bytes[3] != std::byte{'F'}) return 6;
    if (u16(object.bytes, 16) != 1 || u16(object.bytes, 18) != 62) return 7;
    const auto shoff = u64(object.bytes, 40);
    const auto shnum = u16(object.bytes, 60);
    if (shnum != 9 || shoff + static_cast<std::uint64_t>(shnum) * 64U > object.bytes.size()) return 8;
    if (object.stats.section_count != 9 || object.stats.relocation_count < 2 || object.stats.external_symbol_count != 1) return 9;
    const auto duplicate = forge::object::emit_elf64_x86_64(*lowered.module);
    if (!duplicate.ok() || duplicate.bytes != object.bytes) return 10;
    auto bad = *lowered.module;
    bad.functions.push_back(bad.functions.front());
    if (forge::object::emit_elf64_x86_64(bad).ok()) return 11;
    std::cout << "ELF64 deterministic relocatable object: " << object.bytes.size() << " bytes\n";
    return 0;
}
