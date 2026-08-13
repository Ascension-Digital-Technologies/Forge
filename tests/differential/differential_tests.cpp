// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "forge/interpreter/interpreter.hpp"
#include "forge/ir/parser.hpp"
#include "forge/ir/verifier.hpp"
#include "forge/jit/engine.hpp"
#include "forge/machine/lower.hpp"
#include "forge/machine/verifier.hpp"

namespace {

void fail(std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

void print_diagnostics(const forge::Diagnostics& diagnostics) {
    for (const auto& diagnostic : diagnostics) std::cerr << diagnostic.message << '\n';
}

#if defined(_WIN32)
constexpr auto host_abi = forge::codegen::x86_64::Abi::windows;
#else
constexpr auto host_abi = forge::codegen::x86_64::Abi::system_v;
#endif

class DifferentialModule {
public:
    explicit DifferentialModule(std::string_view source) {
        auto parsed = forge::ir::parse_module(source);
        if (!parsed.ok()) {
            print_diagnostics(parsed.diagnostics);
            fail("differential fixture did not parse");
        }
        module_ = std::move(*parsed.module);
        const auto verification = forge::ir::verify_module(module_);
        if (!verification.empty()) {
            print_diagnostics(verification);
            fail("differential fixture did not verify");
        }
        auto lowered = forge::machine::lower_module(module_);
        if (!lowered.ok()) {
            print_diagnostics(lowered.diagnostics);
            fail("differential fixture did not lower");
        }
        const auto machine_verification = forge::machine::verify_module(*lowered.module);
        if (!machine_verification.empty()) {
            print_diagnostics(machine_verification);
            fail("lowered differential fixture did not verify");
        }
        auto loaded = forge::jit::load(*lowered.module, host_abi);
        if (!loaded.ok()) {
            print_diagnostics(loaded.diagnostics);
            fail("differential fixture did not load in the JIT");
        }
        engine_ = std::move(loaded.engine);
    }

    void compare_i64_0(std::string_view name) const {
        const auto expected = interpret(name, forge::ir::TypeKind::i64, {});
        using Function = std::uint64_t (*)();
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing zero-argument i64 JIT entry");
        compare(name, expected, function());
    }

    void compare_i64_1(std::string_view name, std::int64_t argument) const {
        const std::array values{forge::interpreter::Value::integer(
            forge::ir::Type(forge::ir::TypeKind::i64), static_cast<std::uint64_t>(argument))};
        const auto expected = interpret(name, forge::ir::TypeKind::i64, values);
        using Function = std::uint64_t (*)(std::uint64_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing one-argument i64 JIT entry");
        compare(name, expected, function(static_cast<std::uint64_t>(argument)));
    }

    void compare_i64_2(std::string_view name, std::int64_t left, std::int64_t right) const {
        const std::array values{
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i64), static_cast<std::uint64_t>(left)),
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i64), static_cast<std::uint64_t>(right))};
        const auto expected = interpret(name, forge::ir::TypeKind::i64, values);
        using Function = std::uint64_t (*)(std::uint64_t, std::uint64_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing two-argument i64 JIT entry");
        compare(name, expected, function(static_cast<std::uint64_t>(left), static_cast<std::uint64_t>(right)));
    }

    void compare_i64_from_i32(std::string_view name, std::int32_t argument) const {
        const std::array values{forge::interpreter::Value::integer(
            forge::ir::Type(forge::ir::TypeKind::i32), static_cast<std::uint32_t>(argument))};
        const auto expected = interpret(name, forge::ir::TypeKind::i64, values);
        using Function = std::uint64_t (*)(std::uint32_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing i32-to-i64 JIT entry");
        compare(name, expected, function(static_cast<std::uint32_t>(argument)));
    }

    [[nodiscard]] bool is_global_read_only(std::string_view name) const {
        return engine_->is_global_read_only(name);
    }

    [[nodiscard]] void* global_address(std::string_view name) const {
        return engine_->lookup_global(name);
    }

    void compare_i32_from_f64_2(std::string_view name, double left, double right) const {
        const std::array values{
            forge::interpreter::Value::floating(forge::ir::Type(forge::ir::TypeKind::f64), left),
            forge::interpreter::Value::floating(forge::ir::Type(forge::ir::TypeKind::f64), right)};
        const auto expected = interpret(name, forge::ir::TypeKind::i32, values);
        using Function = std::uint32_t (*)(double, double);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing f64 branch JIT entry");
        compare(name, expected, function(left, right));
    }

    void compare_i1_from_f64_2(std::string_view name, double left, double right) const {
        const std::array values{
            forge::interpreter::Value::floating(forge::ir::Type(forge::ir::TypeKind::f64), left),
            forge::interpreter::Value::floating(forge::ir::Type(forge::ir::TypeKind::f64), right)};
        const auto expected = interpret(name, forge::ir::TypeKind::i1, values);
        using Function = std::uint32_t (*)(double, double);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing f64 comparison JIT entry");
        compare(name, expected, function(left, right));
    }

    void compare_void_ptr_i64(std::string_view name, std::span<const std::uint64_t> initial,
                              std::uint64_t delta) const {
        std::vector<std::uint64_t> expected(initial.begin(), initial.end());
        std::vector<std::uint64_t> actual(initial.begin(), initial.end());
        const std::array arguments{
            forge::interpreter::Value::host_pointer(expected.data(), expected.size() * sizeof(std::uint64_t), false),
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i64), delta)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter map-add execution failed"); }
        using Function = void (*)(std::uint64_t*, std::uint64_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing pointer/i64 void JIT entry");
        function(actual.data(), delta);
        require(expected == actual, "packed map-add JIT result disagrees with interpreter");
    }

    void compare_void_ptr_i32(std::string_view name, std::span<const std::uint32_t> initial,
                              std::uint32_t scalar) const {
        std::vector<std::uint32_t> expected(initial.begin(), initial.end());
        std::vector<std::uint32_t> actual(initial.begin(), initial.end());
        const std::array arguments{
            forge::interpreter::Value::host_pointer(expected.data(), expected.size() * sizeof(std::uint32_t), false),
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i32), scalar)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter packed i32 execution failed"); }
        using Function = void (*)(std::uint32_t*, std::uint32_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing pointer/i32 void JIT entry");
        function(actual.data(), scalar);
        require(expected == actual, "packed i32 expression JIT result disagrees with interpreter");
    }

    void compare_void_ptr_ptr_i32(std::string_view name, std::span<const std::uint32_t> source,
                                  std::uint32_t scalar) const {
        std::vector<std::uint32_t> expected_source(source.begin(), source.end());
        std::vector<std::uint32_t> actual_source(source.begin(), source.end());
        std::vector<std::uint32_t> expected_destination(source.size(), 0xdeadbeefU);
        std::vector<std::uint32_t> actual_destination(source.size(), 0xdeadbeefU);
        const std::array arguments{
            forge::interpreter::Value::host_pointer(expected_source.data(), expected_source.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(expected_destination.data(), expected_destination.size() * sizeof(std::uint32_t), false),
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i32), scalar)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter packed copy-map execution failed"); }
        using Function = void (*)(const std::uint32_t*, std::uint32_t*, std::uint32_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing pointer/pointer/i32 void JIT entry");
        function(actual_source.data(), actual_destination.data(), scalar);
        require(expected_destination == actual_destination, "packed copy-map JIT result disagrees with interpreter");
        require(actual_source == expected_source, "packed copy-map unexpectedly mutated its source");
    }

    void compare_void_ptr_ptr_ptr_i32(std::string_view name,
                                      std::span<const std::uint32_t> lhs,
                                      std::span<const std::uint32_t> rhs) const {
        require(lhs.size() == rhs.size(), "vector map input sizes differ");
        std::vector<std::uint32_t> expected_lhs(lhs.begin(), lhs.end());
        std::vector<std::uint32_t> expected_rhs(rhs.begin(), rhs.end());
        std::vector<std::uint32_t> actual_lhs(lhs.begin(), lhs.end());
        std::vector<std::uint32_t> actual_rhs(rhs.begin(), rhs.end());
        std::vector<std::uint32_t> expected_destination(lhs.size(), 0xdeadbeefU);
        std::vector<std::uint32_t> actual_destination(lhs.size(), 0xdeadbeefU);
        const std::array arguments{
            forge::interpreter::Value::host_pointer(expected_lhs.data(), expected_lhs.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(expected_rhs.data(), expected_rhs.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(expected_destination.data(), expected_destination.size() * sizeof(std::uint32_t), false)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter vector-to-vector map execution failed"); }
        using Function = void (*)(const std::uint32_t*, const std::uint32_t*, std::uint32_t*);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing pointer/pointer/pointer void JIT entry");
        function(actual_lhs.data(), actual_rhs.data(), actual_destination.data());
        require(expected_destination == actual_destination, "vector-to-vector map JIT result disagrees with interpreter");
        require(actual_lhs == expected_lhs && actual_rhs == expected_rhs, "vector-to-vector map unexpectedly mutated a source");
    }

    void compare_void_ptr_ptr_ptr_ptr_i32(std::string_view name,
                                          std::span<const std::uint32_t> a,
                                          std::span<const std::uint32_t> b,
                                          std::span<const std::uint32_t> c) const {
        require(a.size() == b.size() && a.size() == c.size(), "chained vector map input sizes differ");
        std::vector<std::uint32_t> ea(a.begin(), a.end()), eb(b.begin(), b.end()), ec(c.begin(), c.end());
        std::vector<std::uint32_t> aa(a.begin(), a.end()), ab(b.begin(), b.end()), ac(c.begin(), c.end());
        std::vector<std::uint32_t> ed(a.size(), 0xdeadbeefU), ad(a.size(), 0xdeadbeefU);
        const std::array arguments{
            forge::interpreter::Value::host_pointer(ea.data(), ea.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(eb.data(), eb.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(ec.data(), ec.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(ed.data(), ed.size() * sizeof(std::uint32_t), false)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter chained vector map execution failed"); }
        using Function = void (*)(const std::uint32_t*, const std::uint32_t*, const std::uint32_t*, std::uint32_t*);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing chained vector map JIT entry");
        function(aa.data(), ab.data(), ac.data(), ad.data());
        require(ed == ad, "chained vector map JIT result disagrees with interpreter");
        require(aa == ea && ab == eb && ac == ec, "chained vector map unexpectedly mutated a source");
    }

    void compare_void_five_sources_i32(std::string_view name,
                                       std::span<const std::uint32_t> a,
                                       std::span<const std::uint32_t> b,
                                       std::span<const std::uint32_t> c,
                                       std::span<const std::uint32_t> d,
                                       std::span<const std::uint32_t> e) const {
        require(a.size() == b.size() && a.size() == c.size() && a.size() == d.size() && a.size() == e.size(),
                "deep chained vector map input sizes differ");
        std::vector<std::uint32_t> ea(a.begin(), a.end()), eb(b.begin(), b.end()), ec(c.begin(), c.end()), ed(d.begin(), d.end()), ee(e.begin(), e.end());
        std::vector<std::uint32_t> aa(a.begin(), a.end()), ab(b.begin(), b.end()), ac(c.begin(), c.end()), ad(d.begin(), d.end()), ae(e.begin(), e.end());
        std::vector<std::uint32_t> expected_destination(a.size(), 0xdeadbeefU), actual_destination(a.size(), 0xdeadbeefU);
        const std::array arguments{
            forge::interpreter::Value::host_pointer(ea.data(), ea.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(eb.data(), eb.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(ec.data(), ec.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(ed.data(), ed.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(ee.data(), ee.size() * sizeof(std::uint32_t), true),
            forge::interpreter::Value::host_pointer(expected_destination.data(), expected_destination.size() * sizeof(std::uint32_t), false)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter deep chained vector map execution failed"); }
        using Function = void (*)(const std::uint32_t*, const std::uint32_t*, const std::uint32_t*, const std::uint32_t*, const std::uint32_t*, std::uint32_t*);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing deep chained vector map JIT entry");
        function(aa.data(), ab.data(), ac.data(), ad.data(), ae.data(), actual_destination.data());
        require(expected_destination == actual_destination, "deep chained vector map JIT result disagrees with interpreter");
        require(aa == ea && ab == eb && ac == ec && ad == ed && ae == ee, "deep chained vector map unexpectedly mutated a source");
    }

    void compare_void_ptr_ptr_ptr_ptr_i64(std::string_view name,
                                          std::span<const std::uint64_t> a,
                                          std::span<const std::uint64_t> b,
                                          std::span<const std::uint64_t> c) const {
        require(a.size() == b.size() && a.size() == c.size(), "wide chained vector map input sizes differ");
        std::vector<std::uint64_t> ea(a.begin(), a.end()), eb(b.begin(), b.end()), ec(c.begin(), c.end());
        std::vector<std::uint64_t> aa(a.begin(), a.end()), ab(b.begin(), b.end()), ac(c.begin(), c.end());
        std::vector<std::uint64_t> ed(a.size(), 0xdeadbeefdeadbeefULL), ad(a.size(), 0xdeadbeefdeadbeefULL);
        const std::array arguments{
            forge::interpreter::Value::host_pointer(ea.data(), ea.size() * sizeof(std::uint64_t), true),
            forge::interpreter::Value::host_pointer(eb.data(), eb.size() * sizeof(std::uint64_t), true),
            forge::interpreter::Value::host_pointer(ec.data(), ec.size() * sizeof(std::uint64_t), true),
            forge::interpreter::Value::host_pointer(ed.data(), ed.size() * sizeof(std::uint64_t), false)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter wide chained vector map execution failed"); }
        using Function = void (*)(const std::uint64_t*, const std::uint64_t*, const std::uint64_t*, std::uint64_t*);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing wide chained vector map JIT entry");
        function(aa.data(), ab.data(), ac.data(), ad.data());
        require(ed == ad, "wide chained vector map JIT result disagrees with interpreter");
        require(aa == ea && ab == eb && ac == ec, "wide chained vector map unexpectedly mutated a source");
    }

    void compare_void_ptr_ptr_ptr_i64(std::string_view name,
                                      std::span<const std::uint64_t> lhs,
                                      std::span<const std::uint64_t> rhs) const {
        require(lhs.size() == rhs.size(), "wide vector map input sizes differ");
        std::vector<std::uint64_t> expected_lhs(lhs.begin(), lhs.end()), expected_rhs(rhs.begin(), rhs.end());
        std::vector<std::uint64_t> actual_lhs(lhs.begin(), lhs.end()), actual_rhs(rhs.begin(), rhs.end());
        std::vector<std::uint64_t> expected_destination(lhs.size(), 0xdeadbeefdeadbeefULL);
        std::vector<std::uint64_t> actual_destination(lhs.size(), 0xdeadbeefdeadbeefULL);
        const std::array arguments{
            forge::interpreter::Value::host_pointer(expected_lhs.data(), expected_lhs.size() * sizeof(std::uint64_t), true),
            forge::interpreter::Value::host_pointer(expected_rhs.data(), expected_rhs.size() * sizeof(std::uint64_t), true),
            forge::interpreter::Value::host_pointer(expected_destination.data(), expected_destination.size() * sizeof(std::uint64_t), false)};
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) { print_diagnostics(result.diagnostics); fail("interpreter wide vector-to-vector map execution failed"); }
        using Function = void (*)(const std::uint64_t*, const std::uint64_t*, std::uint64_t*);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing wide pointer/pointer/pointer void JIT entry");
        function(actual_lhs.data(), actual_rhs.data(), actual_destination.data());
        require(expected_destination == actual_destination, "wide vector-to-vector map JIT result disagrees with interpreter");
        require(actual_lhs == expected_lhs && actual_rhs == expected_rhs, "wide vector-to-vector map unexpectedly mutated a source");
    }

    void compare_i32_ptr(std::string_view name, std::span<const std::uint32_t> values) const {
        const std::array arguments{forge::interpreter::Value::host_pointer(
            const_cast<std::uint32_t*>(values.data()), values.size_bytes(), true)};
        const auto expected = interpret(name, forge::ir::TypeKind::i32, arguments);
        using Function = std::uint32_t (*)(const std::uint32_t*);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing pointer-to-i32 JIT entry");
        compare(name, expected, function(values.data()));
    }

    void compare_i32_2(std::string_view name, std::int32_t left, std::int32_t right) const {
        const std::array values{
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i32), static_cast<std::uint32_t>(left)),
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i32), static_cast<std::uint32_t>(right))};
        const auto expected = interpret(name, forge::ir::TypeKind::i32, values);
        using Function = std::uint32_t (*)(std::uint32_t, std::uint32_t);
        const auto function = reinterpret_cast<Function>(engine_->lookup(name));
        require(function != nullptr, "missing two-argument i32 JIT entry");
        compare(name, expected, function(static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(right)));
    }

private:
    std::uint64_t interpret(std::string_view name, forge::ir::TypeKind kind,
                            std::span<const forge::interpreter::Value> arguments) const {
        auto result = forge::interpreter::execute(module_, name, arguments);
        if (!result.diagnostics.empty()) {
            print_diagnostics(result.diagnostics);
            fail("interpreter execution failed");
        }
        require(result.value.has_value(), "interpreter returned no value");
        require(result.value->kind() == forge::interpreter::Value::Kind::integer,
                "interpreter returned a non-integer value");
        require(result.value->type().kind() == kind, "interpreter returned the wrong integer type");
        return result.value->bits();
    }

    static void compare(std::string_view name, std::uint64_t expected, std::uint64_t actual) {
        if (expected == actual) return;
        std::cerr << "FAIL: differential mismatch in @" << name << ": interpreter="
                  << expected << " JIT=" << actual << '\n';
        std::exit(1);
    }

    forge::ir::Module module_;
    std::unique_ptr<forge::jit::Engine> engine_;
};

} // namespace

int main() {
#if !(defined(__x86_64__) || defined(_M_X64) || defined(__amd64__))
    std::cout << "differential JIT tests skipped: x86-64 host required\n";
    return 0;
#else
    constexpr auto source = R"(module @differential {
array @Words = i16[8]
struct @Record { tag: i8, first: i64, second: i16 }
struct @Inner { value: i64, words: array @Words }
struct @Outer { tag: i8, inner: struct @Inner, tail: i64 }
global @counter: i64 = 41
constant @increment: i64 = 1
constant @message: i8[8] align 8 = "Forge!\0\0"
global @scratch: i8[16] align 16 = zero
global @bulk_source: i8[16] align 8 = zero
global @bulk_destination: i8[16] align 8 = zero
global @typed_outer: struct @Outer = { tag: 7, inner: { value: 99, words: [1, 2, 3, 4, 5, 6, 7, 8] }, tail: 1234 }
constant @typed_words: array @Words = [10, 20, 30, 40, 50, 60, 70, 80]
global @copy_source: struct @Outer = { tag: 9, inner: { value: 77, words: [2, 4, 6, 8, 10, 12, 14, 16] }, tail: 555 }
global @copy_destination: struct @Outer = zero
global @array_source: array @Words = [11, 22, 33, 44, 55, 66, 77, 88]
global @array_destination: array @Words = zero

func @read_outer_tail(%object: struct @Outer) -> i64 {
entry:
  %tail = struct.field.name.address ptr %object @Outer tail
  %value = load i64 %tail align 8
  return %value
}

func @borrow_outer() -> struct @Outer {
entry:
  %object = global.address ptr @typed_outer
  return %object
}

func @read_borrowed_outer() -> i64 {
entry:
  %object = call ptr @borrow_outer()
  %tail = struct.field.name.address ptr %object @Outer tail
  %value = load i64 %tail align 8
  return %value
}

func @make_owned_outer(%input: i64) -> owned struct @Outer {
entry:
  %object = stack.alloc.struct ptr @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %input %tail align 8
  return %object
}

func @read_owned_outer(%input: i64) -> i64 {
entry:
  %object = call ptr @make_owned_outer(%input)
  %tail = struct.field.name.address ptr %object @Outer tail
  %value = load i64 %tail align 8
  return %value
}

func @make_owned_words(%input: i64) -> owned array @Words {
entry:
  %words = stack.alloc.array ptr @Words
  %slot = array.element.address ptr %words @Words 5
  %narrow = truncate i16 %input
  store i16 %narrow %slot align 2
  return %words
}

func @forward_owned_words(%input: i64) -> owned array @Words {
entry:
  %words = call ptr @make_owned_words(%input)
  return %words
}

func @read_forwarded_words(%input: i64) -> i64 {
entry:
  %words = call ptr @forward_owned_words(%input)
  %slot = array.element.address ptr %words @Words 5
  %value = load i16 %slot align 2
  %wide = zero_extend i64 %value
  return %wide
}

func @forward_owned_outer(%input: i64) -> owned struct @Outer {
entry:
  %object = call ptr @make_owned_outer(%input)
  return %object
}

func @read_forwarded_outer(%input: i64) -> i64 {
entry:
  %object = call ptr @forward_owned_outer(%input)
  %tail = struct.field.name.address ptr %object @Outer tail
  %value = load i64 %tail align 8
  return %value
}

func @typed_aggregate_parameter(%value: i64) -> i64 {
entry:
  %object = global.address ptr @typed_outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %value %tail align 8
  %result = call i64 @read_outer_tail(%object)
  return %result
}

func @mutate_owned_outer(%object: owned struct @Outer) -> void {
entry:
  %tail = struct.field.name.address ptr %object @Outer tail
  %replacement = const i64 999
  store i64 %replacement %tail align 8
  return
}

func @owned_struct_parameter(%value: i64) -> i64 {
entry:
  %object = global.address ptr @typed_outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %value %tail align 8
  call void @mutate_owned_outer(%object)
  %result = load i64 %tail align 8
  return %result
}

func @mutate_owned_words(%words: owned array @Words) -> void {
entry:
  %slot = array.element.address ptr %words @Words 5
  %replacement = const i16 1234
  store i16 %replacement %slot align 2
  return
}

func @owned_array_parameter(%value: i64) -> i64 {
entry:
  %words = global.address ptr @array_destination
  %slot = array.element.address ptr %words @Words 5
  %small = truncate i16 %value
  store i16 %small %slot align 2
  call void @mutate_owned_words(%words)
  %loaded = load i16 %slot align 2
  %result = zero_extend i64 %loaded
  return %result
}

func @typed_stack_aggregate(%value: i64) -> i64 {
entry:
  %object = stack.alloc.struct ptr @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %value %tail align 8
  %loaded = load i64 %tail align 8
  return %loaded
}

func @typed_struct_copy_zero() -> i64 {
entry:
  %source = global.address ptr @copy_source
  %destination = global.address ptr @copy_destination
  aggregate.zero.struct void %destination @Outer
  aggregate.copy.struct void %destination %source @Outer
  %tail = struct.field.name.address ptr %destination @Outer tail
  %result = load i64 %tail align 8
  return %result
}

func @typed_array_copy_zero() -> i64 {
entry:
  %source = global.address ptr @array_source
  %destination = global.address ptr @array_destination
  aggregate.zero.array void %destination @Words
  aggregate.copy.array void %destination %source @Words
  %element = array.element.address ptr %destination @Words 6
  %small = load i16 %element align 2
  %result = zero_extend i64 %small
  return %result
}

func @typed_aggregate_global(%value: i64) -> i64 {
entry:
  %base = global.address ptr @typed_outer
  %inner = struct.field.name.address ptr %base @Outer inner
  %words = struct.field.name.address ptr %inner @Inner words
  %element = array.element.address ptr %words @Words 5
  %tail = struct.field.name.address ptr %base @Outer tail
  %small = truncate i16 %value
  store i16 %small %element align 2
  store i64 %value %tail align 8
  %loaded_small = load i16 %element align 2
  %loaded_tail = load i64 %tail align 8
  %wide = zero_extend i64 %loaded_small
  %result = xor i64 %wide %loaded_tail
  return %result
}

func @typed_aggregate_constant() -> i64 {
entry:
  %base = global.address ptr @typed_words
  %element = array.element.address ptr %base @Words 3
  %loaded = load i16 %element align 2
  %result = zero_extend i64 %loaded
  return %result
}

func @constant_word() -> i64 {
entry:
  %address = global.address ptr @message
  %value = load i64 %address align 8
  return %value
}

func @zero_storage() -> i64 {
entry:
  %address = global.address ptr @scratch
  %before = load i64 %address align 8
  %value = const i64 123456789
  store i64 %value %address align 8
  %after = load i64 %address align 8
  %result = add i64 %before %after
  return %result
}

func @arithmetic64(%left: i64, %right: i64) -> i64 {
entry:
  %sum = add i64 %left %right
  %difference = sub i64 %left %right
  %product = mul i64 %sum %difference
  return %product
}

func @signed64(%left: i64, %right: i64) -> i64 {
entry:
  %quotient = div.signed i64 %left %right
  %remainder = rem.signed i64 %left %right
  %combined = add i64 %quotient %remainder
  return %combined
}

func @unsigned64(%left: i64, %right: i64) -> i64 {
entry:
  %quotient = div.unsigned i64 %left %right
  %remainder = rem.unsigned i64 %left %right
  %combined = xor i64 %quotient %remainder
  return %combined
}

func @bitwise64(%left: i64, %right: i64) -> i64 {
entry:
  %both = and i64 %left %right
  %either = or i64 %left %right
  %mixed = xor i64 %both %either
  %inverted = not i64 %mixed
  return %inverted
}

func @shifts64(%value: i64, %amount: i64) -> i64 {
entry:
  %left = shl i64 %value %amount
  %logical = shr.unsigned i64 %left %amount
  %arithmetic = shr.signed i64 %value %amount
  %combined = xor i64 %logical %arithmetic
  return %combined
}

func @select64(%left: i64, %right: i64) -> i64 {
entry:
  %greater = cmp.gt i64 %left %right
  branch %greater, selected(%left), selected(%right)
selected(%value: i64):
  return %value
}

func @sum_to64(%limit: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %zero)
loop(%index: i64, %total: i64):
  %continue = cmp.lt i64 %index %limit
  branch %continue, body(%index, %total), exit(%total)
body(%current: i64, %running: i64):
  %next_total = add i64 %running %current
  %next_index = add i64 %current %one
  jump loop(%next_index, %next_total)
exit(%answer: i64):
  return %answer
}

func @callee64(%left: i64, %right: i64) -> i64 {
entry:
  %sum = add i64 %left %right
  return %sum
}

func @caller64(%left: i64, %right: i64) -> i64 {
entry:
  %sum = call i64 @callee64(%left, %right)
  %two = const i64 2
  %result = mul i64 %sum %two
  return %result
}

func @stack64(%left: i64, %right: i64) -> i64 {
entry:
  %memory = stack.alloc ptr 16 align 16
  %second = ptr.offset ptr %memory 8
  store i64 %left %memory align 8
  store i64 %right %second align 8
  %loaded_left = load i64 %memory align 8
  %loaded_right = load i64 %second align 8
  %result = sub i64 %loaded_left %loaded_right
  return %result
}


func @nested_aggregate(%value: i64) -> i64 {
entry:
  %storage = stack.alloc ptr 40 align 8
  %inner = struct.field.name.address ptr %storage @Outer inner
  %words = struct.field.name.address ptr %inner @Inner words
  %element = array.element.address ptr %words @Words 5
  %tail = struct.field.name.address ptr %storage @Outer tail
  %narrow = truncate i16 %value
  store i16 %narrow %element align 2
  store i64 %value %tail align 8
  %loaded_word = load i16 %element align 2
  %wide_word = zero_extend i64 %loaded_word
  %loaded_tail = load i64 %tail align 8
  %result = xor i64 %wide_word %loaded_tail
  return %result
}

func @array_layout_constants() -> i64 {
entry:
  %size = sizeof.array i64 @Words
  %alignment = alignof.array i64 @Words
  %result = add i64 %size %alignment
  return %result
}

func @array_element(%value: i64) -> i64 {
entry:
  %storage = stack.alloc ptr 16 align 2
  %element = array.element.address ptr %storage @Words 5
  %word = truncate i16 %value
  store i16 %word %element align 2
  %loaded = load i16 %element align 2
  %result = zero_extend i64 %loaded
  return %result
}

func @layout_constants() -> i64 {
entry:
  %size = sizeof.struct i64 @Record
  %alignment = alignof.struct i64 @Record
  %result = add i64 %size %alignment
  return %result
}

func @structured_fields_named(%first: i64, %second: i64) -> i64 {
entry:
  %object = stack.alloc ptr 24 align 8
  %first_field = struct.field.name.address ptr %object @Record first
  %second_field = struct.field.name.address ptr %object @Record second
  store i64 %first %first_field align 8
  store i64 %second %second_field align 2
  %loaded_first = load i64 %first_field align 8
  %loaded_second = load i64 %second_field align 2
  %result = add i64 %loaded_first %loaded_second
  return %result
}

func @structured_fields(%first: i64, %second: i64) -> i64 {
entry:
  %object = stack.alloc ptr 24 align 8
  %first_field = struct.field.address ptr %object @Record 1
  %second_field = struct.field.address ptr %object @Record 2
  store i64 %first %first_field align 8
  store i64 %second %second_field align 8
  %loaded_first = load i64 %first_field align 8
  %loaded_second = load i64 %second_field align 8
  %result = add i64 %loaded_first %loaded_second
  return %result
}

func @global64() -> i64 {
entry:
  %counter_address = global.address ptr @counter
  %increment_address = global.address ptr @increment
  %counter_value = load i64 %counter_address
  %increment_value = load i64 %increment_address
  %updated = add i64 %counter_value %increment_value
  store i64 %updated %counter_address
  return %updated
}

func @mixed32(%left: i32, %right: i32) -> i32 {
entry:
  %sum = add i32 %left %right
  %product = mul i32 %sum %left
  %one = const i32 1
  %shifted = shr.signed i32 %product %one
  %result = xor i32 %shifted %right
  return %result
}

func @zero_extend32(%value: i32) -> i64 {
entry:
  %result = zero_extend i64 %value
  return %result
}

func @sign_extend32(%value: i32) -> i64 {
entry:
  %result = sign_extend i64 %value
  return %result
}

func @truncate_zero_extend8(%value: i64) -> i64 {
entry:
  %small = truncate i8 %value
  %result = zero_extend i64 %small
  return %result
}

func @truncate_sign_extend8(%value: i64) -> i64 {
entry:
  %small = truncate i8 %value
  %result = sign_extend i64 %small
  return %result
}

func @narrow_stack(%value: i64) -> i64 {
entry:
  %memory = stack.alloc ptr 4 align 2
  %word_address = ptr.offset ptr %memory 2
  %byte = truncate i8 %value
  %word = truncate i16 %value
  store i8 %byte %memory align 1
  store i16 %word %word_address align 2
  %loaded_byte = load i8 %memory align 1
  %loaded_word = load i16 %word_address align 2
  %wide_byte = zero_extend i64 %loaded_byte
  %wide_word = zero_extend i64 %loaded_word
  %result = add i64 %wide_byte %wide_word
  return %result
}

func @narrow_signed_stack(%value: i64) -> i64 {
entry:
  %memory = stack.alloc ptr 2 align 2
  %byte = truncate i8 %value
  store i8 %byte %memory align 1
  %loaded = load i8 %memory align 1
  %result = sign_extend i64 %loaded
  return %result
}

func @narrow_global(%value: i64) -> i64 {
entry:
  %base = global.address ptr @scratch
  %word_address = ptr.offset ptr %base 2
  %byte = truncate i8 %value
  %word = truncate i16 %value
  store i8 %byte %base align 1
  store i16 %word %word_address align 2
  %loaded_byte = load i8 %base align 1
  %loaded_word = load i16 %word_address align 2
  %wide_byte = zero_extend i64 %loaded_byte
  %wide_word = zero_extend i64 %loaded_word
  %result = xor i64 %wide_byte %wide_word
  return %result
}

func @bulk_fill_copy(%value: i64) -> i64 {
entry:
  %source = global.address ptr @bulk_source
  %destination = global.address ptr @bulk_destination
  %byte = truncate i8 %value
  %count = const i64 16
  memory.set void %source %byte %count
  memory.copy void %destination %source %count
  %result = load i64 %destination align 8
  return %result
}

func @bulk_overlap() -> i64 {
entry:
  %memory = global.address ptr @bulk_source
  %destination = ptr.offset ptr %memory 2
  %pattern = const i64 72623859790382856
  %count = const i64 8
  store i64 %pattern %memory align 8
  memory.copy void %destination %memory %count
  %result = load i64 %memory align 8
  return %result
}

func @borrowed_aggregate_block(%input: i64) -> i64 {
entry:
  %object = stack.alloc.struct ptr @Outer
  aggregate.zero.struct void %object @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %input %tail align 8
  %zero = const i64 0
  %condition = cmp.ge i64 %input %zero
  branch %condition, positive(%object), negative(%object)
positive(%positive_value: struct @Outer):
  jump join(%positive_value)
negative(%negative_value: struct @Outer):
  jump join(%negative_value)
join(%selected: struct @Outer):
  %selected_tail = struct.field.name.address ptr %selected @Outer tail
  %result = load i64 %selected_tail align 8
  return %result
}

func @owned_aggregate_block(%input: i64) -> i64 {
entry:
  %object = stack.alloc.struct ptr @Outer
  aggregate.zero.struct void %object @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %input %tail align 8
  jump mutate(%object)
mutate(%copy: owned struct @Outer):
  %copy_tail = struct.field.name.address ptr %copy @Outer tail
  %replacement = const i64 999
  store i64 %replacement %copy_tail align 8
  %original_tail = struct.field.name.address ptr %object @Outer tail
  %result = load i64 %original_tail align 8
  return %result
}

func @move_aggregate(%input: i64) -> i64 {
entry:
  %object = stack.alloc.struct ptr @Outer
  aggregate.zero.struct void %object @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %input %tail align 8
  %moved = aggregate.move.struct ptr %object @Outer
  %moved_tail = struct.field.name.address ptr %moved @Outer tail
  %result = load i64 %moved_tail align 8
  return %result
}

func @move_into_loop(%input: i64) -> i64 {
entry:
  %object = stack.alloc.struct ptr @Outer
  aggregate.zero.struct void %object @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %input %tail align 8
  %moved = aggregate.move.struct ptr %object @Outer
  %zero = const i64 0
  jump loop(%moved, %zero)
loop(%state: struct @Outer, %index: i64):
  %one = const i64 1
  %continue = cmp.lt i64 %index %one
  branch %continue, body(%state, %index), exit(%state)
body(%body_state: struct @Outer, %body_index: i64):
  %next = add i64 %body_index %one
  jump loop(%body_state, %next)
exit(%final_state: struct @Outer):
  %final_tail = struct.field.name.address ptr %final_state @Outer tail
  %result = load i64 %final_tail align 8
  return %result
}

func @end_after_read(%input: i64) -> i64 {
entry:
  %object = stack.alloc.struct ptr @Outer
  aggregate.zero.struct void %object @Outer
  %tail = struct.field.name.address ptr %object @Outer tail
  store i64 %input %tail align 8
  %result = load i64 %tail align 8
  aggregate.end.struct void %object @Outer
  return %result
}
})";

    const DifferentialModule module(source);
    require(module.is_global_read_only("message"), "constant global was not placed in read-only storage");
    require(!module.is_global_read_only("scratch"), "writable global was marked read-only");
    require(module.global_address("message") != nullptr, "constant global address is missing");
    require(module.global_address("scratch") != nullptr, "writable global address is missing");
    require(module.global_address("message") != module.global_address("scratch"), "read-only and writable globals overlap");
    require(module.global_address("typed_outer") != nullptr, "typed structure global address is missing");
    require(module.global_address("typed_words") != nullptr, "typed array constant address is missing");
    require(module.is_global_read_only("typed_words"), "typed array constant was not protected");

    for (const auto& [left, right] : std::array<std::pair<std::int64_t, std::int64_t>, 4>{
             std::pair<std::int64_t, std::int64_t>{9, 4},
             std::pair<std::int64_t, std::int64_t>{-17, 5},
             std::pair<std::int64_t, std::int64_t>{5'000'000'000LL, 71},
             std::pair<std::int64_t, std::int64_t>{-9'000'000'000LL, -13}}) {
        module.compare_i64_2("arithmetic64", left, right);
        module.compare_i64_2("signed64", left, right);
        module.compare_i64_2("bitwise64", left, right);
        module.compare_i64_2("select64", left, right);
        module.compare_i64_2("caller64", left, right);
        module.compare_i64_2("stack64", left, right);
        module.compare_i64_2("structured_fields", left, right);
        module.compare_i64_2("structured_fields_named", left, right);
    }

    module.compare_i64_2("unsigned64", static_cast<std::int64_t>(0xf000000000000123ULL), 97);
    module.compare_i64_2("unsigned64", 5'000'000'000LL, 71);
    module.compare_i64_2("shifts64", -9'223'372'036'854'775'000LL, 7);
    module.compare_i64_2("shifts64", 0x123456789abcdefLL, 13);

    for (const auto limit : std::array<std::int64_t, 4>{0, 1, 10, 1'000}) {
        module.compare_i64_1("sum_to64", limit);
    }

    module.compare_i64_0("global64");
    module.compare_i64_0("constant_word");
    module.compare_i64_0("zero_storage");
    module.compare_i64_0("layout_constants");
    module.compare_i64_0("array_layout_constants");
    module.compare_i64_0("typed_aggregate_constant");
    module.compare_i64_0("read_borrowed_outer");
    module.compare_i64_0("bulk_overlap");
    module.compare_i64_0("typed_struct_copy_zero");
    module.compare_i64_0("typed_array_copy_zero");
    for (const auto value : std::array<std::int64_t, 4>{0, 0x1234, 0xffff, 0x1234'5678'9abc'deffLL}) module.compare_i64_1("array_element", value);

    for (const auto value : std::array<std::int32_t, 4>{0, 127, -1, -2'000'000'000}) {
        module.compare_i64_from_i32("zero_extend32", value);
        module.compare_i64_from_i32("sign_extend32", value);
    }
    for (const auto value : std::array<std::int64_t, 5>{0, 0x7f, 0x80, 0xff, 0x1234'5678'9abc'deffLL}) {
        module.compare_i64_1("truncate_zero_extend8", value);
        module.compare_i64_1("truncate_sign_extend8", value);
    }
    for (const auto value : std::array<std::int64_t, 6>{0, 0x7f, 0x80, 0xff, 0x1234, 0x1234'5678'9abc'deffLL}) {
        module.compare_i64_1("narrow_stack", value);
        module.compare_i64_1("narrow_signed_stack", value);
        module.compare_i64_1("narrow_global", value);
        module.compare_i64_1("nested_aggregate", value);
        module.compare_i64_1("typed_aggregate_global", value);
        module.compare_i64_1("read_owned_outer", value);
        module.compare_i64_1("read_forwarded_outer", value);
        module.compare_i64_1("read_forwarded_words", value);
        module.compare_i64_1("bulk_fill_copy", value);
        module.compare_i64_1("typed_stack_aggregate", value);
        module.compare_i64_1("typed_aggregate_parameter", value);
        module.compare_i64_1("owned_struct_parameter", value);
        module.compare_i64_1("owned_array_parameter", value);
        module.compare_i64_1("borrowed_aggregate_block", value);
        module.compare_i64_1("owned_aggregate_block", value);
        module.compare_i64_1("move_aggregate", value);
        module.compare_i64_1("move_into_loop", value);
        module.compare_i64_1("end_after_read", value);
    }

    for (const auto& [left, right] : std::array<std::pair<std::int32_t, std::int32_t>, 3>{
             std::pair<std::int32_t, std::int32_t>{7, 3},
             std::pair<std::int32_t, std::int32_t>{-1'000, 19},
             std::pair<std::int32_t, std::int32_t>{123'456, -789}}) {
        module.compare_i32_2("mixed32", left, right);
    }



    constexpr auto borrow_module = R"(module @borrow_runtime {
struct @Record { value: i64 }
func @borrow_read(%input: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %input %field align 8
  %borrow = aggregate.borrow.struct ptr %record @Record
  %borrowed_field = struct.field.name.address ptr %borrow @Record value
  %result = load i64 %borrowed_field align 8
  aggregate.borrow.end.struct void %borrow @Record
  aggregate.end.struct void %record @Record
  return %result
}
})";
    DifferentialModule borrow_fixture(borrow_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL})
        borrow_fixture.compare_i64_1("borrow_read", value);

    constexpr auto borrowed_call_module = R"(module @borrowed_call_runtime {
struct @Record { value: i64 }
func @read_record(%record: borrow struct @Record) -> i64 {
entry:
  %field = struct.field.name.address ptr %record @Record value
  %result = load i64 %field align 8
  return %result
}
func @write_record(%record: borrow mut struct @Record, %value: i64) -> i64 {
entry:
  %child = aggregate.borrow.mut.struct ptr %record @Record
  %field = struct.field.name.address ptr %child @Record value
  store i64 %value %field align 8
  aggregate.borrow.end.struct void %child @Record
  %parent_field = struct.field.name.address ptr %record @Record value
  %result = load i64 %parent_field align 8
  return %result
}
func @borrowed_calls(%value: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %written = call i64 @write_record(%record, %value)
  %read = call i64 @read_record(%record)
  %result = add i64 %written %read
  return %result
}
})";
    DifferentialModule borrowed_call_fixture(borrowed_call_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL})
        borrowed_call_fixture.compare_i64_1("borrowed_calls", value);
    constexpr auto conditional_borrowed_return_module = R"(module @conditional_borrowed_return_runtime {
struct @Record { value: i64 }
func @select(%condition: i1, %record: borrow struct @Record) -> borrow struct @Record from 1 {
entry:
  branch %condition, left(%record), right(%record)
left(%left_record: struct @Record):
  jump join(%left_record)
right(%right_record: struct @Record):
  jump join(%right_record)
join(%selected: struct @Record):
  return %selected
}
func @read_selected(%value: i64, %condition: i1) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  %loan = call ptr @select(%condition, %record)
  %selected_field = struct.field.name.address ptr %loan @Record value
  %result = load i64 %selected_field align 8
  aggregate.borrow.end.struct void %loan @Record
  return %result
}
})";
    DifferentialModule conditional_borrowed_return_fixture(conditional_borrowed_return_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL}) {
        conditional_borrowed_return_fixture.compare_i64_2("read_selected", value, 0);
        conditional_borrowed_return_fixture.compare_i64_2("read_selected", value, 1);
    }


    constexpr auto deep_borrowed_return_module = R"(module @deep_borrowed_return_runtime {
struct @Record { value: i64 }
func @identity(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  return %record
}
func @forward_once(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  %loan = call ptr @identity(%record)
  return %loan
}
func @forward_twice(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  %loan = call ptr @forward_once(%record)
  return %loan
}
func @loop_forward(%record: borrow struct @Record, %count: i64) -> borrow struct @Record from 0 {
entry:
  jump loop(%record, %count)
loop(%loan: struct @Record, %remaining: i64):
  %zero = const i64 0
  %continue = cmp.gt i64 %remaining %zero
  branch %continue, body(%loan, %remaining), done(%loan)
body(%body_loan: struct @Record, %body_remaining: i64):
  %one = const i64 1
  %next = sub i64 %body_remaining %one
  jump loop(%body_loan, %next)
done(%result_loan: struct @Record):
  return %result_loan
}
func @read_deep(%value: i64, %count: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  %first = call ptr @forward_twice(%record)
  aggregate.borrow.end.struct void %first @Record
  %second = call ptr @loop_forward(%record, %count)
  %second_field = struct.field.name.address ptr %second @Record value
  %result = load i64 %second_field align 8
  aggregate.borrow.end.struct void %second @Record
  return %result
}
})";
    DifferentialModule deep_borrowed_return_fixture(deep_borrowed_return_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL}) {
        deep_borrowed_return_fixture.compare_i64_2("read_deep", value, 0);
        deep_borrowed_return_fixture.compare_i64_2("read_deep", value, 4);
    }


    constexpr auto independent_signature_module = R"(module @independent_signature_runtime {
signature @Unary(%value: i64) -> i64
func @increment(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = add i64 %value %one
  return %result
}
func @dispatch(%value: i64) -> i64 {
entry:
  %target = func.address ptr @increment as @Unary
  %result = call.indirect i64 %target as @Unary(%value)
  return %result
}
})";
    DifferentialModule independent_signature_fixture(independent_signature_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL})
        independent_signature_fixture.compare_i64_1("dispatch", value);

    constexpr auto indirect_borrowed_module = R"(module @indirect_borrowed_runtime {
struct @Record { value: i64 }
func @identity(%record: borrow struct @Record) -> borrow struct @Record from 0 {
entry:
  return %record
}
func @read_indirect(%value: i64) -> i64 {
entry:
  %record = stack.alloc.struct ptr @Record
  aggregate.zero.struct void %record @Record
  %field = struct.field.name.address ptr %record @Record value
  store i64 %value %field align 8
  %target = func.address ptr @identity
  %loan = call.indirect ptr %target as @identity(%record)
  %loan_field = struct.field.name.address ptr %loan @Record value
  %result = load i64 %loan_field align 8
  aggregate.borrow.end.struct void %loan @Record
  return %result
}
})";
    DifferentialModule indirect_borrowed_fixture(indirect_borrowed_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL})
        indirect_borrowed_fixture.compare_i64_1("read_indirect", value);


    constexpr auto indirect_owned_module = R"(module @indirect_owned_runtime {
struct @Pair { first: i64, second: i64 }
func @make_pair(%value: i64) -> owned struct @Pair {
entry:
  %pair = stack.alloc.struct ptr @Pair
  aggregate.zero.struct void %pair @Pair
  %first = struct.field.name.address ptr %pair @Pair first
  %second = struct.field.name.address ptr %pair @Pair second
  %seven = const i64 7
  store i64 %value %first align 8
  store i64 %seven %second align 8
  return %pair
}
func @mutate_copy(%pair: owned struct @Pair) -> void {
entry:
  %first = struct.field.name.address ptr %pair @Pair first
  %replacement = const i64 999
  store i64 %replacement %first align 8
  return
}
func @read_owned_return(%value: i64) -> i64 {
entry:
  %target = func.address ptr @make_pair
  %pair = call.indirect ptr %target as @make_pair(%value)
  %first = struct.field.name.address ptr %pair @Pair first
  %second = struct.field.name.address ptr %pair @Pair second
  %a = load i64 %first align 8
  %b = load i64 %second align 8
  %result = add i64 %a %b
  return %result
}
func @preserve_owned_argument(%value: i64) -> i64 {
entry:
  %pair = stack.alloc.struct ptr @Pair
  aggregate.zero.struct void %pair @Pair
  %first = struct.field.name.address ptr %pair @Pair first
  store i64 %value %first align 8
  %target = func.address ptr @mutate_copy
  call.indirect void %target as @mutate_copy(%pair)
  %result = load i64 %first align 8
  return %result
}
})";
    DifferentialModule indirect_owned_fixture(indirect_owned_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL}) {
        indirect_owned_fixture.compare_i64_1("read_owned_return", value);
        indirect_owned_fixture.compare_i64_1("preserve_owned_argument", value);
    }


    constexpr auto floating_compare_module = R"(module @floating_compare_runtime {
func @ordered_less(%left: f64, %right: f64) -> i32 {
entry:
  %condition = cmp.lt f64 %left %right
  branch %condition, yes(), no()
yes:
  %one = const i32 1
  return %one
no:
  %zero = const i32 0
  return %zero
}
func @equal_value(%left: f64, %right: f64) -> i1 {
entry:
  %condition = cmp.eq f64 %left %right
  return %condition
}
func @not_equal_value(%left: f64, %right: f64) -> i1 {
entry:
  %condition = cmp.ne f64 %left %right
  return %condition
}
})";
    DifferentialModule floating_compare_fixture(floating_compare_module);
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    floating_compare_fixture.compare_i32_from_f64_2("ordered_less", 1.0, 2.0);
    floating_compare_fixture.compare_i32_from_f64_2("ordered_less", 2.0, 1.0);
    floating_compare_fixture.compare_i32_from_f64_2("ordered_less", nan, 1.0);
    floating_compare_fixture.compare_i1_from_f64_2("equal_value", 4.0, 4.0);
    floating_compare_fixture.compare_i1_from_f64_2("equal_value", nan, nan);
    floating_compare_fixture.compare_i1_from_f64_2("not_equal_value", 4.0, 4.0);
    floating_compare_fixture.compare_i1_from_f64_2("not_equal_value", nan, nan);

    constexpr auto callback_global_module = R"(module @callback_global_runtime {
signature @Unary(%value: i64) -> i64
global @dispatch_slot: callback @Unary align 8 = zero
func @increment(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = add i64 %value %one
  return %result
}
func @dispatch(%value: i64) -> i64 {
entry:
  %slot = global.address ptr @dispatch_slot
  %target = func.address ptr @increment as @Unary
  store ptr %target %slot
  %loaded = load ptr %slot
  %result = call.indirect i64 %loaded as @Unary(%value)
  return %result
}
})";
    DifferentialModule callback_global_fixture(callback_global_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL})
        callback_global_fixture.compare_i64_1("dispatch", value);


    constexpr auto callback_table_module = R"(module @callback_table_runtime {
signature @Unary(%value: i64) -> i64
global @dispatch_table: callback @Unary[2] align 8 = zero
func @increment(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = add i64 %value %one
  return %result
}
func @decrement(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = sub i64 %value %one
  return %result
}
func @dispatch_first(%value: i64) -> i64 {
entry:
  %table = global.address ptr @dispatch_table
  %slot0 = callback.element.address ptr %table @Unary 0
  %slot1 = callback.element.address ptr %table @Unary 1
  %increment = func.address ptr @increment as @Unary
  %decrement = func.address ptr @decrement as @Unary
  store ptr %increment %slot0
  store ptr %decrement %slot1
  %target = load ptr %slot0
  %result = call.indirect i64 %target as @Unary(%value)
  return %result
}
func @dispatch_second(%value: i64) -> i64 {
entry:
  %table = global.address ptr @dispatch_table
  %slot0 = callback.element.address ptr %table @Unary 0
  %slot1 = callback.element.address ptr %table @Unary 1
  %increment = func.address ptr @increment as @Unary
  %decrement = func.address ptr @decrement as @Unary
  store ptr %increment %slot0
  store ptr %decrement %slot1
  %target = load ptr %slot1
  %result = call.indirect i64 %target as @Unary(%value)
  return %result
}
})";
    DifferentialModule callback_table_fixture(callback_table_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL}) {
        callback_table_fixture.compare_i64_1("dispatch_first", value);
        callback_table_fixture.compare_i64_1("dispatch_second", value);
    }

    constexpr auto dynamic_callback_table_module = R"(module @dynamic_callback_table_runtime {
signature @Unary(%value: i64) -> i64
global @dispatch_table: callback @Unary[2] align 8 = zero
func @increment(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = add i64 %value %one
  return %result
}
func @decrement(%value: i64) -> i64 {
entry:
  %one = const i64 1
  %result = sub i64 %value %one
  return %result
}
func @dispatch(%value: i64, %index: i64) -> i64 {
entry:
  %table = global.address ptr @dispatch_table
  %slot0 = callback.element.address ptr %table @Unary 0
  %slot1 = callback.element.address ptr %table @Unary 1
  %increment = func.address ptr @increment as @Unary
  %decrement = func.address ptr @decrement as @Unary
  store ptr %increment %slot0
  store ptr %decrement %slot1
  %selected = callback.element.address ptr %table @Unary %index
  %target = load ptr %selected
  %result = call.indirect i64 %target as @Unary(%value)
  return %result
}
})";
    DifferentialModule dynamic_callback_table_fixture(dynamic_callback_table_module);
    for (const auto value : std::array<std::int64_t, 4>{0, 1, -1, 0x1234'5678'9abc'def0LL}) {
        dynamic_callback_table_fixture.compare_i64_2("dispatch", value, 0);
        dynamic_callback_table_fixture.compare_i64_2("dispatch", value, 1);
    }

    constexpr auto slp_map_module = R"(module @slp_map_runtime {
func @add4(%base: ptr, %delta: i64) -> void {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %v0 = load i64 %base
  %v1 = load i64 %p1
  %v2 = load i64 %p2
  %v3 = load i64 %p3
  %r0 = add i64 %v0 %delta
  %r1 = add i64 %v1 %delta
  %r2 = add i64 %v2 %delta
  %r3 = add i64 %v3 %delta
  store i64 %r0 %base
  store i64 %r1 %p1
  store i64 %r2 %p2
  store i64 %r3 %p3
  return
}
})";
    DifferentialModule slp_map_fixture(slp_map_module);
    const std::array<std::uint64_t, 4> slp_map_small{1U, 2U, 3U, 4U};
    const std::array<std::uint64_t, 4> slp_map_wrap{0xfffffffffffffff0ULL, 7ULL, 0x8000000000000000ULL, 99ULL};
    slp_map_fixture.compare_void_ptr_i64("add4", slp_map_small, 5U);
    slp_map_fixture.compare_void_ptr_i64("add4", slp_map_wrap, 0x20U);

    constexpr auto slp_expression_pack_module = R"(module @slp_expression_pack_runtime {
func @xor4_i32(%base: ptr, %mask: i32) -> void {
entry:
  %p1 = ptr.offset ptr %base 4
  %p2 = ptr.offset ptr %base 8
  %p3 = ptr.offset ptr %base 12
  %v0 = load i32 %base
  %v1 = load i32 %p1
  %v2 = load i32 %p2
  %v3 = load i32 %p3
  %r0 = xor i32 %v0 %mask
  %r1 = xor i32 %v1 %mask
  %r2 = xor i32 %v2 %mask
  %r3 = xor i32 %v3 %mask
  store i32 %r0 %base
  store i32 %r1 %p1
  store i32 %r2 %p2
  store i32 %r3 %p3
  return
}
func @and4_i64(%base: ptr, %mask: i64) -> void {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %v0 = load i64 %base
  %v1 = load i64 %p1
  %v2 = load i64 %p2
  %v3 = load i64 %p3
  %r0 = and i64 %v0 %mask
  %r1 = and i64 %v1 %mask
  %r2 = and i64 %v2 %mask
  %r3 = and i64 %v3 %mask
  store i64 %r0 %base
  store i64 %r1 %p1
  store i64 %r2 %p2
  store i64 %r3 %p3
  return
}
func @sub4_i64(%base: ptr, %delta: i64) -> void {
entry:
  %p1 = ptr.offset ptr %base 8
  %p2 = ptr.offset ptr %base 16
  %p3 = ptr.offset ptr %base 24
  %v0 = load i64 %base
  %v1 = load i64 %p1
  %v2 = load i64 %p2
  %v3 = load i64 %p3
  %r0 = sub i64 %v0 %delta
  %r1 = sub i64 %v1 %delta
  %r2 = sub i64 %v2 %delta
  %r3 = sub i64 %v3 %delta
  store i64 %r0 %base
  store i64 %r1 %p1
  store i64 %r2 %p2
  store i64 %r3 %p3
  return
}
})";
    DifferentialModule slp_expression_pack_fixture(slp_expression_pack_module);
    const std::array<std::uint32_t, 4> slp_xor_values{0U, 0xffffffffU, 0x80000000U, 0x12345678U};
    slp_expression_pack_fixture.compare_void_ptr_i32("xor4_i32", slp_xor_values, 0xa5a55a5aU);
    const std::array<std::uint64_t, 4> slp_and_values{0ULL, 0xffffffffffffffffULL, 0x8000000000000000ULL, 0x123456789abcdef0ULL};
    slp_expression_pack_fixture.compare_void_ptr_i64("and4_i64", slp_and_values, 0x00ff00ff00ff00ffULL);
    slp_expression_pack_fixture.compare_void_ptr_i64("sub4_i64", slp_and_values, 0x1020304050607080ULL);


    constexpr auto slp_i32_two_lane_module = R"(module @slp_i32_two_lane_runtime {
func @xor2_i32(%base: ptr, %mask: i32) -> void {
entry:
  %p1 = ptr.offset ptr %base 4
  %v0 = load i32 %base
  %v1 = load i32 %p1
  %r0 = xor i32 %v0 %mask
  %r1 = xor i32 %v1 %mask
  store i32 %r0 %base
  store i32 %r1 %p1
  return
}
})";
    DifferentialModule slp_i32_two_lane_fixture(slp_i32_two_lane_module);
    const std::array<std::uint32_t, 4> slp_i32_two_lane_canary{0x11223344U, 0x55667788U, 0xa5a5a5a5U, 0x5a5a5a5aU};
    slp_i32_two_lane_fixture.compare_void_ptr_i32("xor2_i32", slp_i32_two_lane_canary, 0x0f0ff0f0U);

    constexpr auto slp_copy_map_module = R"(module @slp_copy_map_runtime {
func @xor_copy4_i32(%src: ptr, %dst: ptr, %mask: i32) -> void {
entry:
  %s1 = ptr.offset ptr %src 4
  %s2 = ptr.offset ptr %src 8
  %s3 = ptr.offset ptr %src 12
  %d1 = ptr.offset ptr %dst 4
  %d2 = ptr.offset ptr %dst 8
  %d3 = ptr.offset ptr %dst 12
  %v0 = load i32 %src
  %v1 = load i32 %s1
  %v2 = load i32 %s2
  %v3 = load i32 %s3
  %r0 = xor i32 %v0 %mask
  %r1 = xor i32 %v1 %mask
  %r2 = xor i32 %v2 %mask
  %r3 = xor i32 %v3 %mask
  store i32 %r0 %dst
  store i32 %r1 %d1
  store i32 %r2 %d2
  store i32 %r3 %d3
  return
}
})";
    DifferentialModule slp_copy_map_fixture(slp_copy_map_module);
    slp_copy_map_fixture.compare_void_ptr_ptr_i32("xor_copy4_i32", slp_xor_values, 0x0f0ff0f0U);

    constexpr auto slp_vector_map_module = R"(module @slp_vector_map_runtime {
func @add4_i32(%lhs: ptr, %rhs: ptr, %dst: ptr) -> void {
entry:
  %l1 = ptr.offset ptr %lhs 4
  %l2 = ptr.offset ptr %lhs 8
  %l3 = ptr.offset ptr %lhs 12
  %r1 = ptr.offset ptr %rhs 4
  %r2 = ptr.offset ptr %rhs 8
  %r3 = ptr.offset ptr %rhs 12
  %d1 = ptr.offset ptr %dst 4
  %d2 = ptr.offset ptr %dst 8
  %d3 = ptr.offset ptr %dst 12
  %a0 = load i32 %lhs
  %a1 = load i32 %l1
  %a2 = load i32 %l2
  %a3 = load i32 %l3
  %b0 = load i32 %rhs
  %b1 = load i32 %r1
  %b2 = load i32 %r2
  %b3 = load i32 %r3
  %v0 = add i32 %a0 %b0
  %v1 = add i32 %a1 %b1
  %v2 = add i32 %a2 %b2
  %v3 = add i32 %a3 %b3
  store i32 %v0 %dst
  store i32 %v1 %d1
  store i32 %v2 %d2
  store i32 %v3 %d3
  return
}
})";
    DifferentialModule slp_vector_map_fixture(slp_vector_map_module);
    const std::array<std::uint32_t, 4> slp_vector_rhs{7U, 0x80000000U, 0xffffffffU, 0x76543210U};
    slp_vector_map_fixture.compare_void_ptr_ptr_ptr_i32("add4_i32", slp_xor_values, slp_vector_rhs);

    constexpr auto slp_vector_sub_module = R"(module @slp_vector_sub_runtime {
func @sub4_i64(%lhs: ptr, %rhs: ptr, %dst: ptr) -> void {
entry:
  %l1 = ptr.offset ptr %lhs 8
  %l2 = ptr.offset ptr %lhs 16
  %l3 = ptr.offset ptr %lhs 24
  %r1 = ptr.offset ptr %rhs 8
  %r2 = ptr.offset ptr %rhs 16
  %r3 = ptr.offset ptr %rhs 24
  %d1 = ptr.offset ptr %dst 8
  %d2 = ptr.offset ptr %dst 16
  %d3 = ptr.offset ptr %dst 24
  %a0 = load i64 %lhs
  %a1 = load i64 %l1
  %a2 = load i64 %l2
  %a3 = load i64 %l3
  %b0 = load i64 %rhs
  %b1 = load i64 %r1
  %b2 = load i64 %r2
  %b3 = load i64 %r3
  %v0 = sub i64 %a0 %b0
  %v1 = sub i64 %a1 %b1
  %v2 = sub i64 %a2 %b2
  %v3 = sub i64 %a3 %b3
  store i64 %v0 %dst
  store i64 %v1 %d1
  store i64 %v2 %d2
  store i64 %v3 %d3
  return
}
})";
    DifferentialModule slp_vector_sub_fixture(slp_vector_sub_module);
    const std::array<std::uint64_t, 4> slp_vector_lhs64{0ULL, 1ULL, 0x8000000000000000ULL, 0xffffffffffffffffULL};
    const std::array<std::uint64_t, 4> slp_vector_rhs64{1ULL, 3ULL, 0xffffffffffffffffULL, 0x123456789abcdef0ULL};
    slp_vector_sub_fixture.compare_void_ptr_ptr_ptr_i64("sub4_i64", slp_vector_lhs64, slp_vector_rhs64);

    constexpr auto slp_chained_module = R"(module @slp_chained_runtime {
func @chain4_i32(%a: ptr, %b: ptr, %c: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %d1 = ptr.offset ptr %dst 4
  %d2 = ptr.offset ptr %dst 8
  %d3 = ptr.offset ptr %dst 12
  %av0 = load i32 %a
  %av1 = load i32 %a1
  %av2 = load i32 %a2
  %av3 = load i32 %a3
  %bv0 = load i32 %b
  %bv1 = load i32 %b1
  %bv2 = load i32 %b2
  %bv3 = load i32 %b3
  %cv0 = load i32 %c
  %cv1 = load i32 %c1
  %cv2 = load i32 %c2
  %cv3 = load i32 %c3
  %x0 = xor i32 %av0 %bv0
  %x1 = xor i32 %av1 %bv1
  %x2 = xor i32 %av2 %bv2
  %x3 = xor i32 %av3 %bv3
  %v0 = add i32 %x0 %cv0
  %v1 = add i32 %x1 %cv1
  %v2 = add i32 %x2 %cv2
  %v3 = add i32 %x3 %cv3
  store i32 %v0 %dst
  store i32 %v1 %d1
  store i32 %v2 %d2
  store i32 %v3 %d3
  return
}
})";
    DifferentialModule slp_chained_fixture(slp_chained_module);
    const std::array<std::uint32_t, 4> chain_a{0U, 1U, 0xffffffffU, 0x80000000U};
    const std::array<std::uint32_t, 4> chain_b{0xffffffffU, 3U, 0x12345678U, 0x7fffffffU};
    const std::array<std::uint32_t, 4> chain_c{1U, 0xffffffffU, 0xeeeeeeeeU, 0x80000000U};
    slp_chained_fixture.compare_void_ptr_ptr_ptr_ptr_i32("chain4_i32", chain_a, chain_b, chain_c);

    constexpr auto slp_chained_i64_module = R"(module @slp_chained_i64_runtime {
func @chain4_i64(%a: ptr, %b: ptr, %c: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 8
  %a2 = ptr.offset ptr %a 16
  %a3 = ptr.offset ptr %a 24
  %b1 = ptr.offset ptr %b 8
  %b2 = ptr.offset ptr %b 16
  %b3 = ptr.offset ptr %b 24
  %c1 = ptr.offset ptr %c 8
  %c2 = ptr.offset ptr %c 16
  %c3 = ptr.offset ptr %c 24
  %d1 = ptr.offset ptr %dst 8
  %d2 = ptr.offset ptr %dst 16
  %d3 = ptr.offset ptr %dst 24
  %av0 = load i64 %a
  %av1 = load i64 %a1
  %av2 = load i64 %a2
  %av3 = load i64 %a3
  %bv0 = load i64 %b
  %bv1 = load i64 %b1
  %bv2 = load i64 %b2
  %bv3 = load i64 %b3
  %cv0 = load i64 %c
  %cv1 = load i64 %c1
  %cv2 = load i64 %c2
  %cv3 = load i64 %c3
  %x0 = sub i64 %av0 %bv0
  %x1 = sub i64 %av1 %bv1
  %x2 = sub i64 %av2 %bv2
  %x3 = sub i64 %av3 %bv3
  %v0 = xor i64 %x0 %cv0
  %v1 = xor i64 %x1 %cv1
  %v2 = xor i64 %x2 %cv2
  %v3 = xor i64 %x3 %cv3
  store i64 %v0 %dst
  store i64 %v1 %d1
  store i64 %v2 %d2
  store i64 %v3 %d3
  return
}
})";
    DifferentialModule slp_chained_i64_fixture(slp_chained_i64_module);
    const std::array<std::uint64_t, 4> chain64_a{0ULL, 1ULL, 0xffffffffffffffffULL, 0x8000000000000000ULL};
    const std::array<std::uint64_t, 4> chain64_b{1ULL, 3ULL, 0x123456789abcdef0ULL, 0x7fffffffffffffffULL};
    const std::array<std::uint64_t, 4> chain64_c{0xffffffffffffffffULL, 7ULL, 0x0f0f0f0f0f0f0f0fULL, 0xaaaaaaaaaaaaaaaaULL};
    slp_chained_i64_fixture.compare_void_ptr_ptr_ptr_ptr_i64("chain4_i64", chain64_a, chain64_b, chain64_c);

    constexpr auto slp_deep_chain_module = R"(module @slp_deep_chain_runtime {
func @chain4ops_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %o1 = ptr.offset ptr %dst 4
  %o2 = ptr.offset ptr %dst 8
  %o3 = ptr.offset ptr %dst 12
  %av0 = load i32 %a
  %av1 = load i32 %a1
  %av2 = load i32 %a2
  %av3 = load i32 %a3
  %bv0 = load i32 %b
  %bv1 = load i32 %b1
  %bv2 = load i32 %b2
  %bv3 = load i32 %b3
  %cv0 = load i32 %c
  %cv1 = load i32 %c1
  %cv2 = load i32 %c2
  %cv3 = load i32 %c3
  %dv0 = load i32 %d
  %dv1 = load i32 %d1
  %dv2 = load i32 %d2
  %dv3 = load i32 %d3
  %ev0 = load i32 %e
  %ev1 = load i32 %e1
  %ev2 = load i32 %e2
  %ev3 = load i32 %e3
  %x0 = xor i32 %av0 %bv0
  %x1 = xor i32 %av1 %bv1
  %x2 = xor i32 %av2 %bv2
  %x3 = xor i32 %av3 %bv3
  %y0 = add i32 %x0 %cv0
  %y1 = add i32 %x1 %cv1
  %y2 = add i32 %x2 %cv2
  %y3 = add i32 %x3 %cv3
  %z0 = and i32 %y0 %dv0
  %z1 = and i32 %y1 %dv1
  %z2 = and i32 %y2 %dv2
  %z3 = and i32 %y3 %dv3
  %v0 = sub i32 %z0 %ev0
  %v1 = sub i32 %z1 %ev1
  %v2 = sub i32 %z2 %ev2
  %v3 = sub i32 %z3 %ev3
  store i32 %v0 %dst
  store i32 %v1 %o1
  store i32 %v2 %o2
  store i32 %v3 %o3
  return
}
})";
    DifferentialModule slp_deep_chain_fixture(slp_deep_chain_module);
    const std::array<std::uint32_t, 4> deep_a{0U, 1U, 0xffffffffU, 0x80000000U};
    const std::array<std::uint32_t, 4> deep_b{0xffffffffU, 3U, 0x12345678U, 0x7fffffffU};
    const std::array<std::uint32_t, 4> deep_c{1U, 0xffffffffU, 0xeeeeeeeeU, 0x80000000U};
    const std::array<std::uint32_t, 4> deep_d{0x0f0f0f0fU, 0xf0f0f0f0U, 0xaaaaaaaaU, 0x55555555U};
    const std::array<std::uint32_t, 4> deep_e{7U, 11U, 0xffffffffU, 0x80000001U};
    slp_deep_chain_fixture.compare_void_five_sources_i32("chain4ops_i32", deep_a, deep_b, deep_c, deep_d, deep_e);

    constexpr auto slp_branching_dag_module = R"(module @slp_branching_dag_runtime {
func @branching_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %o1 = ptr.offset ptr %dst 4
  %o2 = ptr.offset ptr %dst 8
  %o3 = ptr.offset ptr %dst 12
  %av0 = load i32 %a
  %av1 = load i32 %a1
  %av2 = load i32 %a2
  %av3 = load i32 %a3
  %bv0 = load i32 %b
  %bv1 = load i32 %b1
  %bv2 = load i32 %b2
  %bv3 = load i32 %b3
  %cv0 = load i32 %c
  %cv1 = load i32 %c1
  %cv2 = load i32 %c2
  %cv3 = load i32 %c3
  %dv0 = load i32 %d
  %dv1 = load i32 %d1
  %dv2 = load i32 %d2
  %dv3 = load i32 %d3
  %ev0 = load i32 %e
  %ev1 = load i32 %e1
  %ev2 = load i32 %e2
  %ev3 = load i32 %e3
  %l0 = xor i32 %av0 %bv0
  %l1 = xor i32 %av1 %bv1
  %l2 = xor i32 %av2 %bv2
  %l3 = xor i32 %av3 %bv3
  %r0 = and i32 %cv0 %dv0
  %r1 = and i32 %cv1 %dv1
  %r2 = and i32 %cv2 %dv2
  %r3 = and i32 %cv3 %dv3
  %m0 = add i32 %l0 %r0
  %m1 = add i32 %l1 %r1
  %m2 = add i32 %l2 %r2
  %m3 = add i32 %l3 %r3
  %v0 = sub i32 %m0 %ev0
  %v1 = sub i32 %m1 %ev1
  %v2 = sub i32 %m2 %ev2
  %v3 = sub i32 %m3 %ev3
  store i32 %v0 %dst
  store i32 %v1 %o1
  store i32 %v2 %o2
  store i32 %v3 %o3
  return
}
})";
    DifferentialModule slp_branching_dag_fixture(slp_branching_dag_module);
    slp_branching_dag_fixture.compare_void_five_sources_i32("branching_i32", deep_a, deep_b, deep_c, deep_d, deep_e);

    constexpr auto slp_shared_dag_module = R"(module @slp_shared_dag_runtime {
func @shared_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {
entry:
  %a1 = ptr.offset ptr %a 4
  %a2 = ptr.offset ptr %a 8
  %a3 = ptr.offset ptr %a 12
  %b1 = ptr.offset ptr %b 4
  %b2 = ptr.offset ptr %b 8
  %b3 = ptr.offset ptr %b 12
  %c1 = ptr.offset ptr %c 4
  %c2 = ptr.offset ptr %c 8
  %c3 = ptr.offset ptr %c 12
  %d1 = ptr.offset ptr %d 4
  %d2 = ptr.offset ptr %d 8
  %d3 = ptr.offset ptr %d 12
  %e1 = ptr.offset ptr %e 4
  %e2 = ptr.offset ptr %e 8
  %e3 = ptr.offset ptr %e 12
  %o1 = ptr.offset ptr %dst 4
  %o2 = ptr.offset ptr %dst 8
  %o3 = ptr.offset ptr %dst 12
  %av0 = load i32 %a
  %av1 = load i32 %a1
  %av2 = load i32 %a2
  %av3 = load i32 %a3
  %bv0 = load i32 %b
  %bv1 = load i32 %b1
  %bv2 = load i32 %b2
  %bv3 = load i32 %b3
  %cv0 = load i32 %c
  %cv1 = load i32 %c1
  %cv2 = load i32 %c2
  %cv3 = load i32 %c3
  %dv0 = load i32 %d
  %dv1 = load i32 %d1
  %dv2 = load i32 %d2
  %dv3 = load i32 %d3
  %ev0 = load i32 %e
  %ev1 = load i32 %e1
  %ev2 = load i32 %e2
  %ev3 = load i32 %e3
  %s0 = xor i32 %av0 %bv0
  %s1 = xor i32 %av1 %bv1
  %s2 = xor i32 %av2 %bv2
  %s3 = xor i32 %av3 %bv3
  %l0 = add i32 %s0 %cv0
  %l1 = add i32 %s1 %cv1
  %l2 = add i32 %s2 %cv2
  %l3 = add i32 %s3 %cv3
  %r0 = and i32 %s0 %dv0
  %r1 = and i32 %s1 %dv1
  %r2 = and i32 %s2 %dv2
  %r3 = and i32 %s3 %dv3
  %m0 = add i32 %l0 %r0
  %m1 = add i32 %l1 %r1
  %m2 = add i32 %l2 %r2
  %m3 = add i32 %l3 %r3
  %v0 = xor i32 %m0 %ev0
  %v1 = xor i32 %m1 %ev1
  %v2 = xor i32 %m2 %ev2
  %v3 = xor i32 %m3 %ev3
  store i32 %v0 %dst
  store i32 %v1 %o1
  store i32 %v2 %o2
  store i32 %v3 %o3
  return
}
})";
    DifferentialModule slp_shared_dag_fixture(slp_shared_dag_module);
    slp_shared_dag_fixture.compare_void_five_sources_i32("shared_i32", deep_a, deep_b, deep_c, deep_d, deep_e);


    const auto make_pressure_dag_source = [](bool interleave_stores) {
        std::string source = "module @slp_pressure_dag_runtime {\nfunc @pressure_i32(%a: ptr, %b: ptr, %c: ptr, %d: ptr, %e: ptr, %dst: ptr) -> void {\nentry:\n";
        constexpr std::size_t lanes = 4U;
        const std::array<std::string_view, 6> bases{"a", "b", "c", "d", "e", "dst"};
        for (const auto base : bases) {
            for (std::size_t lane = 1; lane < lanes; ++lane)
                source += "  %" + std::string(base) + std::to_string(lane) + " = ptr.offset ptr %" + std::string(base) + " " + std::to_string(lane * 4U) + "\n";
        }
        for (const auto base : std::array<std::string_view, 5>{"a", "b", "c", "d", "e"}) {
            for (std::size_t lane = 0; lane < lanes; ++lane)
                source += "  %" + std::string(base) + "v" + std::to_string(lane) + " = load i32 %" + std::string(base) + (lane == 0U ? "" : std::to_string(lane)) + "\n";
        }
        const std::array<std::string_view, 5> operations{"xor", "and", "or", "add", "sub"};
        for (std::size_t op = 0; op < operations.size(); ++op)
            for (std::size_t lane = 0; lane < lanes; ++lane)
                source += "  %s" + std::to_string(op) + "_" + std::to_string(lane) + " = " + std::string(operations[op]) + " i32 %av" + std::to_string(lane) + " %bv" + std::to_string(lane) + "\n";
        for (std::size_t op = 0; op < operations.size(); ++op)
            for (std::size_t lane = 0; lane < lanes; ++lane)
                source += "  %t" + std::to_string(op) + "_" + std::to_string(lane) + " = " + std::string(operations[op]) + " i32 %cv" + std::to_string(lane) + " %dv" + std::to_string(lane) + "\n";
        constexpr std::array<std::size_t, 5> rotated{1U, 2U, 3U, 4U, 0U};
        for (std::size_t op = 0; op < operations.size(); ++op)
            for (std::size_t lane = 0; lane < lanes; ++lane)
                source += "  %p" + std::to_string(op) + "_" + std::to_string(lane) + " = add i32 %s" + std::to_string(op) + "_" + std::to_string(lane) + " %t" + std::to_string(op) + "_" + std::to_string(lane) + "\n";
        for (std::size_t op = 0; op < operations.size(); ++op)
            for (std::size_t lane = 0; lane < lanes; ++lane)
                source += "  %q" + std::to_string(op) + "_" + std::to_string(lane) + " = xor i32 %s" + std::to_string(op) + "_" + std::to_string(lane) + " %t" + std::to_string(rotated[op]) + "_" + std::to_string(lane) + "\n";
        for (std::size_t lane = 0; lane < lanes; ++lane) {
            source += "  %pa0_" + std::to_string(lane) + " = add i32 %p0_" + std::to_string(lane) + " %p1_" + std::to_string(lane) + "\n";
            source += "  %pa1_" + std::to_string(lane) + " = add i32 %pa0_" + std::to_string(lane) + " %p2_" + std::to_string(lane) + "\n";
            source += "  %pa2_" + std::to_string(lane) + " = add i32 %pa1_" + std::to_string(lane) + " %p3_" + std::to_string(lane) + "\n";
            source += "  %psum_" + std::to_string(lane) + " = add i32 %pa2_" + std::to_string(lane) + " %p4_" + std::to_string(lane) + "\n";
            source += "  %qa0_" + std::to_string(lane) + " = add i32 %q0_" + std::to_string(lane) + " %q1_" + std::to_string(lane) + "\n";
            source += "  %qa1_" + std::to_string(lane) + " = add i32 %qa0_" + std::to_string(lane) + " %q2_" + std::to_string(lane) + "\n";
            source += "  %qa2_" + std::to_string(lane) + " = add i32 %qa1_" + std::to_string(lane) + " %q3_" + std::to_string(lane) + "\n";
            source += "  %qsum_" + std::to_string(lane) + " = add i32 %qa2_" + std::to_string(lane) + " %q4_" + std::to_string(lane) + "\n";
            source += "  %mix_" + std::to_string(lane) + " = xor i32 %psum_" + std::to_string(lane) + " %qsum_" + std::to_string(lane) + "\n";
            source += "  %v" + std::to_string(lane) + " = add i32 %mix_" + std::to_string(lane) + " %ev" + std::to_string(lane) + "\n";
            if (interleave_stores)
                source += "  store i32 %v" + std::to_string(lane) + " %" +
                          std::string(lane == 0U ? "dst" : "dst" + std::to_string(lane)) + "\n";
        }
        if (!interleave_stores)
            for (std::size_t lane = 0; lane < lanes; ++lane)
                source += "  store i32 %v" + std::to_string(lane) + " %" +
                          std::string(lane == 0U ? "dst" : "dst" + std::to_string(lane)) + "\n";
        source += "  return\n}\n}";
        return source;
    };
    const auto pressure_dag_source = make_pressure_dag_source(false);
    DifferentialModule slp_pressure_dag_fixture(pressure_dag_source);
    slp_pressure_dag_fixture.compare_void_five_sources_i32("pressure_i32", deep_a, deep_b, deep_c, deep_d, deep_e);

    // Keep the same high-pressure arithmetic but deliberately interleave the
    // stores so the SLP matcher cannot collapse the function into a packed
    // DAG. This exercises the scalar spill path that once corrupted a live
    // pointer by using allocated r8/r9 as supposedly-private spill-cache
    // registers. The result must remain correct even when both cache slots are
    // unavailable under register pressure.
    const auto scalar_pressure_source = make_pressure_dag_source(true);
    DifferentialModule scalar_pressure_fixture(scalar_pressure_source);
    scalar_pressure_fixture.compare_void_five_sources_i32("pressure_i32", deep_a, deep_b, deep_c, deep_d, deep_e);

    constexpr auto slp_i32_module = R"(module @slp_i32_runtime {
func @sum8(%base: ptr) -> i32 {
entry:
  %p1 = ptr.offset ptr %base 4
  %p2 = ptr.offset ptr %base 8
  %p3 = ptr.offset ptr %base 12
  %p4 = ptr.offset ptr %base 16
  %p5 = ptr.offset ptr %base 20
  %p6 = ptr.offset ptr %base 24
  %p7 = ptr.offset ptr %base 28
  %v0 = load i32 %base
  %v1 = load i32 %p1
  %v2 = load i32 %p2
  %v3 = load i32 %p3
  %v4 = load i32 %p4
  %v5 = load i32 %p5
  %v6 = load i32 %p6
  %v7 = load i32 %p7
  %a0 = add i32 %v0 %v1
  %a1 = add i32 %v2 %v3
  %a2 = add i32 %v4 %v5
  %a3 = add i32 %v6 %v7
  %b0 = add i32 %a0 %a1
  %b1 = add i32 %a2 %a3
  %sum = add i32 %b0 %b1
  return %sum
}
})";
    DifferentialModule slp_i32_fixture(slp_i32_module);
    const std::array<std::uint32_t, 8> slp_i32_small{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    const std::array<std::uint32_t, 8> slp_i32_wrap{
        0xfffffff0U, 0x20U, 0x80000000U, 0x80000000U, 7U, 9U, 11U, 13U};
    slp_i32_fixture.compare_i32_ptr("sum8", slp_i32_small);
    slp_i32_fixture.compare_i32_ptr("sum8", slp_i32_wrap);

    constexpr auto multi_recurrence_module = R"(module @multi_recurrence_runtime {
func @fib(%n: i64) -> i64 {
entry:
  %zero = const i64 0
  %one = const i64 1
  jump loop(%zero, %zero, %one)
loop(%i: i64, %a: i64, %b: i64):
  %done = cmp.ge i64 %i %n
  branch %done, exit(%a), body(%i, %a, %b)
body(%j: i64, %x: i64, %y: i64):
  %sum = add i64 %x %y
  %next = add i64 %j %one
  jump loop(%next, %y, %sum)
exit(%result: i64):
  return %result
}
})";
    DifferentialModule multi_recurrence_fixture(multi_recurrence_module);
    for (const auto value : std::array<std::int64_t, 7>{0, 1, 2, 10, 20, 50, 90})
        multi_recurrence_fixture.compare_i64_1("fib", value);

    std::cout << "interpreter/JIT differential tests passed\n";
    return 0;
#endif
}
