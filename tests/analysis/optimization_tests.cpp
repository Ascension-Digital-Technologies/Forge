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
        std::cout << "Forge optimization tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
