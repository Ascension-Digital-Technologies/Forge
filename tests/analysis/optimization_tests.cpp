// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include "forge/interpreter/interpreter.hpp"
#include "forge/ir/parser.hpp"
#include "forge/ir/printer.hpp"
#include "forge/ir/verifier.hpp"
#include "forge/pass/pass.hpp"
#include "forge/transforms/scalar.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr auto source = R"(module @optimization {
func @compute(%value: i32) -> i32 {
entry:
  %zero = const i32 0
  %one = const i32 1
  %always = const i1 1
  %left = add i32 %value %zero
  %right = mul i32 %left %one
  %first = add i32 %right %one
  %duplicate = add i32 %right %one
  branch %always, live(%first, %duplicate), dead(%zero)
live(%a: i32, %b: i32):
  %answer = add i32 %a %b
  return %answer
dead(%unused: i32):
  %noise = mul i32 %unused %unused
  return %noise
}
})";
        auto parsed = forge::ir::parse_module(source);
        require(parsed.ok(), "optimization fixture failed to parse");
        const std::vector<forge::interpreter::Value> arguments{
            forge::interpreter::Value::integer(forge::ir::Type(forge::ir::TypeKind::i32), 20)};
        const auto before = forge::interpreter::execute(*parsed.module, "compute", arguments);
        require(before.value.has_value() && before.diagnostics.empty() && before.value->signed_value() == 42, "pre-optimization result mismatch");

        forge::pass::PassManager pipeline;
        pipeline.add<forge::transforms::SparseConditionalConstantPropagationPass>()
                .add<forge::transforms::AlgebraicSimplificationPass>()
                .add<forge::transforms::CopyPropagationPass>()
                .add<forge::transforms::CommonSubexpressionEliminationPass>()
                .add<forge::transforms::CopyPropagationPass>()
                .add<forge::transforms::DeadCodeEliminationPass>()
                .add<forge::transforms::SimplifyCFGPass>();
        const auto stats = pipeline.run(*parsed.module);
        require(stats.changed, "optimizer reported no changes");
        require(stats.blocks_removed >= 1, "SCCP did not remove dead branch");
        require(stats.operations_rewritten >= 3, "expected algebraic/CSE rewrites");

        const auto diagnostics = forge::ir::verify_module(*parsed.module);
        for (const auto& diagnostic : diagnostics)
            require(diagnostic.severity != forge::DiagnosticSeverity::error, "optimized module failed verification");
        const auto after = forge::interpreter::execute(*parsed.module, "compute", arguments);
        require(after.value.has_value() && after.diagnostics.empty() && after.value->signed_value() == 42, "post-optimization result mismatch");

        const auto text = forge::ir::print_module(*parsed.module);
        require(text.find("dead(") == std::string::npos, "dead block remains after SCCP");

        constexpr auto memory_source = R"(module @memory_forward {
func @forward() -> i64 {
entry:
  %left = stack.alloc ptr 8 align 8
  %right = stack.alloc ptr 8 align 8
  %ten = const i64 10
  %twenty = const i64 20
  store i64 %ten %left align 8
  %first = load i64 %left align 8
  store i64 %twenty %right align 8
  %second = load i64 %left align 8
  %sum = add i64 %first %second
  return %sum
}
})";
        auto memory = forge::ir::parse_module(memory_source);
        require(memory.ok(), "memory-forwarding fixture failed to parse");
        forge::pass::PassManager memory_pipeline;
        memory_pipeline.add<forge::transforms::MemoryForwardingPass>()
                       .add<forge::transforms::CopyPropagationPass>()
                       .add<forge::transforms::DeadCodeEliminationPass>();
        const auto memory_stats = memory_pipeline.run(*memory.module);
        require(memory_stats.operations_rewritten >= 2, "store/load forwarding did not rewrite both loads");
        const auto memory_text = forge::ir::print_module(*memory.module);
        require(memory_text.find("load i64 %left") == std::string::npos, "forwarded loads remain in IR");
        const auto memory_result = forge::interpreter::execute(*memory.module, "forward", {});
        require(memory_result.value.has_value() && memory_result.value->signed_value() == 20, "memory forwarding changed semantics");

        constexpr auto loop_source = R"(module @licm {
func @loop(%start: i64) -> i64 {
entry:
  %limit = const i64 8
  jump header(%start)
header(%value: i64):
  %step = const i64 2
  %next = add i64 %value %step
  %again = cmp.lt i64 %next %limit
  branch %again, header(%next), exit(%next)
exit(%result: i64):
  return %result
}
})";
        auto loop = forge::ir::parse_module(loop_source);
        require(loop.ok(), "LICM fixture failed to parse");
        auto& loop_function = loop.module->functions().front();
        forge::analysis::FunctionAnalysisManager loop_analyses(loop_function);
        forge::transforms::LoopInvariantCodeMotionPass licm;
        const auto licm_stats = licm.run(loop_function, loop_analyses);
        require(licm_stats.changed && licm_stats.operations_rewritten >= 1, "LICM did not hoist invariant operation");
        const auto loop_text = forge::ir::print_module(*loop.module);
        const auto entry_position = loop_text.find("%step = const i64 2");
        const auto header_position = loop_text.find("header(");
        require(entry_position != std::string::npos && entry_position < header_position, "LICM did not move invariant into preheader");
        const std::vector<forge::interpreter::Value> loop_arguments{
            forge::interpreter::Value::integer(forge::ir::i64_type(), 0)};
        const auto loop_result = forge::interpreter::execute(*loop.module, "loop", loop_arguments);
        require(loop_result.value.has_value() && loop_result.value->signed_value() == 8, "LICM changed loop semantics");
        std::cout << "Forge optimization tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
