#include "forge/analysis/function_analysis.hpp"
#include "forge/ir/parser.hpp"

#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr auto source = R"(module @analysis {
func @diamond(%condition: i1) -> i32 {
entry:
  %one = const i32 1
  branch %condition, left(%one), right(%one)
left(%value_left: i32):
  jump merge(%value_left)
right(%value_right: i32):
  jump merge(%value_right)
merge(%result: i32):
  return %result
}
})";
        auto parsed = forge::ir::parse_module(source);
        require(parsed.ok(), "analysis fixture failed to parse");
        const auto& function = parsed.module->functions().front();
        const auto cfg = forge::analysis::build_cfg(function);
        require(cfg.reachable.size() == 4, "CFG reachability is incorrect");
        require(cfg.predecessors.at("merge").size() == 2, "CFG predecessor count is incorrect");

        const auto uses = forge::analysis::build_use_def(function);
        require(uses.use_count.at("%one") == 2, "use count is incorrect");
        require(uses.definitions.contains("%result"), "block parameter definition missing");

        const auto dominators = forge::analysis::build_dominator_tree(function, cfg);
        require(dominators.dominates("entry", "merge"), "entry should dominate merge");
        require(!dominators.dominates("left", "merge"), "left must not dominate merge");
        std::cout << "Forge analysis tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
