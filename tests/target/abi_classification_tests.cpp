// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include "forge/target/abi.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    forge::ir::Module module("abi-classification");
    module.structs().push_back({"Small", {{"value", forge::ir::i32_type()}}, false});
    module.structs().push_back({"PairI64", {{"left", forge::ir::i64_type()}, {"right", forge::ir::i64_type()}}, false});
    module.structs().push_back({"PairF64", {{"left", forge::ir::f64_type()}, {"right", forge::ir::f64_type()}}, false});
    module.structs().push_back({"Mixed", {{"number", forge::ir::f64_type()}, {"tag", forge::ir::i64_type()}}, false});
    module.structs().push_back({"Large", {{"a", forge::ir::i64_type()}, {"b", forge::ir::i64_type()}, {"c", forge::ir::i64_type()}}, false});

    using forge::target::AbiValueClass;
    using forge::target::NativeAbi;
    const auto small_windows = forge::target::classify_aggregate(module, forge::ir::AggregateRefKind::structure,
        "Small", NativeAbi::windows_x64);
    require(small_windows.has_value(), "small Windows aggregate classification missing");
    require(small_windows->register_passed(), "4-byte Windows aggregate should be register passed");
    require(small_windows->classes[0] == AbiValueClass::integer, "small Windows aggregate should use integer class");

    const auto pair_i64 = forge::target::classify_aggregate(module, forge::ir::AggregateRefKind::structure,
        "PairI64", NativeAbi::system_v_x86_64);
    require(pair_i64.has_value(), "PairI64 classification missing");
    require(pair_i64->register_count == 2, "PairI64 should use two System V eightbytes");
    require(pair_i64->classes[0] == AbiValueClass::integer && pair_i64->classes[1] == AbiValueClass::integer,
            "PairI64 should use two integer classes");

    const auto pair_f64 = forge::target::classify_aggregate(module, forge::ir::AggregateRefKind::structure,
        "PairF64", NativeAbi::system_v_x86_64);
    require(pair_f64.has_value(), "PairF64 classification missing");
    require(pair_f64->classes[0] == AbiValueClass::sse && pair_f64->classes[1] == AbiValueClass::sse,
            "PairF64 should use two SSE classes");

    const auto mixed = forge::target::classify_aggregate(module, forge::ir::AggregateRefKind::structure,
        "Mixed", NativeAbi::system_v_x86_64);
    require(mixed.has_value(), "mixed aggregate classification missing");
    require(mixed->classes[0] == AbiValueClass::sse && mixed->classes[1] == AbiValueClass::integer,
            "mixed aggregate classes are incorrect");

    const auto large_system_v = forge::target::classify_aggregate(module, forge::ir::AggregateRefKind::structure,
        "Large", NativeAbi::system_v_x86_64);
    require(large_system_v.has_value() && large_system_v->passed_indirectly,
            "large System V aggregate should be passed in memory");
    const auto pair_windows = forge::target::classify_aggregate(module, forge::ir::AggregateRefKind::structure,
        "PairI64", NativeAbi::windows_x64);
    require(pair_windows.has_value() && pair_windows->passed_indirectly,
            "16-byte Windows aggregate should be indirect");

    forge::ir::Function function;
    function.name = "mixed_call";
    function.return_type = forge::ir::i64_type();
    function.variadic = true;
    forge::ir::ValueDecl aggregate{"%pair", forge::ir::ptr_type()};
    aggregate.aggregate_kind = forge::ir::AggregateRefKind::structure;
    aggregate.aggregate_name = "PairF64";
    function.parameters.push_back(aggregate);
    function.parameters.push_back({"%tag", forge::ir::i64_type()});
    const auto function_abi = forge::target::classify_function(module, function, NativeAbi::system_v_x86_64);
    require(function_abi.variadic, "function variadic marker was lost");
    require(function_abi.floating_registers == 2, "PairF64 should consume two floating registers");
    require(function_abi.integer_registers == 1, "tag should consume one integer register");

    std::cout << "ABI classification tests passed\n";
}
