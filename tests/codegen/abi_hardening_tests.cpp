#include "forge/codegen/x86_64/encoder.hpp"
#include "forge/ir/parser.hpp"
#include "forge/ir/verifier.hpp"
#include "forge/machine/lower.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "failure: " << message << '\n';
        std::exit(1);
    }
}

const forge::codegen::x86_64::EncodedFunction& find_function(
    const forge::codegen::x86_64::ImageEncodeResult& encoded,
    std::string_view name) {
    for (const auto& function : encoded.functions)
        if (function.name == name) return function;
    std::cerr << "failure: missing encoded function " << name << '\n';
    std::exit(1);
}
}

int main() {
    const auto parsed = forge::ir::parse_module(R"(
module @abi_hardening {
  func @abi_stress(%i0: i64, %a: f64, %i1: i64, %b: f64, %c: f64, %d: f64) -> f64 {
  entry:
    %result = call f64 @mixed_weighted(%i1, %d, %i0, %c, %b, %a)
    return %result
  }
  func @mixed_weighted(%i0: i64, %a: f64, %i1: i64, %b: f64, %c: f64, %d: f64) -> f64 {
  entry:
    %ten = const f64 10.0
    %hundred = const f64 100.0
    %thousand = const f64 1000.0
    %wb = mul f64 %b %ten
    %wc = mul f64 %c %hundred
    %wd = mul f64 %d %thousand
    %ab = add f64 %a %wb
    %abc = add f64 %ab %wc
    %result = add f64 %abc %wd
    return %result
  }
}
)");
    require(parsed.ok(), "ABI fixture did not parse");
    require(forge::ir::verify_module(*parsed.module).empty(), "ABI fixture failed IR verification");
    const auto lowered = forge::machine::lower_module(*parsed.module);
    require(lowered.ok(), "ABI fixture failed machine lowering");

    const auto sysv = forge::codegen::x86_64::encode_image(
        *lowered.module, forge::codegen::x86_64::Abi::system_v);
    require(sysv.ok(), "System V ABI encoding failed");
    const auto& sysv_stress = find_function(sysv, "abi_stress");
    require(sysv_stress.abi_register_argument_snapshot_count == 6,
            "System V call did not snapshot all register arguments");
    require(sysv_stress.abi_stack_argument_count == 0,
            "System V unexpectedly stack-passed the six mixed arguments");
    require(sysv_stress.abi_shadow_space_byte_count == 0,
            "System V emitted Windows shadow space");
    require(sysv_stress.abi_mixed_class_call_count == 1,
            "System V mixed-class call was not counted");

    const auto windows = forge::codegen::x86_64::encode_image(
        *lowered.module, forge::codegen::x86_64::Abi::windows);
    require(windows.ok(), "Windows x64 ABI encoding failed");
    const auto& windows_stress = find_function(windows, "abi_stress");
    require(windows_stress.abi_register_argument_snapshot_count == 4,
            "Windows x64 did not snapshot the four register arguments");
    require(windows_stress.abi_stack_argument_count == 2,
            "Windows x64 did not place arguments five and six on the stack");
    require(windows_stress.abi_shadow_space_byte_count == 32,
            "Windows x64 did not reserve exactly 32 bytes of shadow space");
    require(windows_stress.abi_mixed_class_call_count == 1,
            "Windows x64 mixed-class call was not counted");
    require((windows_stress.abi_alignment_padding_byte_count & 7U) == 0U,
            "Windows x64 call padding is not slot aligned");
    return 0;
}
