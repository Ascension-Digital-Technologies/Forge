// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include "forge/ir/parser.hpp"
#include "forge/ir/verifier.hpp"
#include "forge/machine/lower.hpp"
#include "forge/object/coff.hpp"

namespace {
std::uint16_t u16(const std::vector<std::byte>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes.at(offset))) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes.at(offset + 1))) << 8U;
}
std::uint32_t u32(const std::vector<std::byte>& bytes, std::size_t offset) {
    std::uint32_t value{};
    for (unsigned shift = 0; shift < 32; shift += 8)
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes.at(offset + shift / 8U))) << shift;
    return value;
}
}

int main() {
    const std::string source = R"(
module @coff_test {
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
    auto object = forge::object::emit_coff_x86_64(*lowered.module);
    if (!object.ok()) return 4;
    if (object.bytes.size() < 20 + 3 * 40) return 5;
    if (u16(object.bytes, 0) != 0x8664 || u16(object.bytes, 2) != 3) return 6;
    const auto symbol_table = u32(object.bytes, 8);
    const auto symbol_count = u32(object.bytes, 12);
    if (symbol_count < 3 || symbol_table + symbol_count * 18U + 4U > object.bytes.size()) return 7;
    const auto text_relocations = u16(object.bytes, 20 + 32);
    if (text_relocations < 2) return 8;
    if (object.stats.section_count != 3 || object.stats.relocation_count < 2 || object.stats.external_symbol_count != 1) return 9;
    const auto duplicate = forge::object::emit_coff_x86_64(*lowered.module);
    if (!duplicate.ok() || duplicate.bytes != object.bytes) return 10;
    auto bad = *lowered.module;
    bad.functions.push_back(bad.functions.front());
    if (forge::object::emit_coff_x86_64(bad).ok()) return 11;
    std::cout << "COFF AMD64 deterministic relocatable object: " << object.bytes.size() << " bytes\n";
    return 0;
}
