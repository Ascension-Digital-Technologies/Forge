#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "forge/ir/module.hpp"

namespace forge::analysis {

struct ControlFlowGraph {
    std::unordered_map<std::string, std::vector<std::string>> successors;
    std::unordered_map<std::string, std::vector<std::string>> predecessors;
    std::unordered_set<std::string> reachable;
};

struct UseDefInfo {
    struct Definition {
        std::string block;
        std::size_t operation_index{};
        bool is_parameter{};
    };
    std::unordered_map<std::string, Definition> definitions;
    std::unordered_map<std::string, std::size_t> use_count;
};

struct DominatorTree {
    std::unordered_map<std::string, std::unordered_set<std::string>> dominators;

    [[nodiscard]] bool dominates(const std::string& candidate,
                                 const std::string& block) const;
};

[[nodiscard]] ControlFlowGraph build_cfg(const ir::Function& function);
[[nodiscard]] UseDefInfo build_use_def(const ir::Function& function);
[[nodiscard]] DominatorTree build_dominator_tree(const ir::Function& function,
                                                 const ControlFlowGraph& cfg);

class FunctionAnalysisManager {
public:
    explicit FunctionAnalysisManager(const ir::Function& function)
        : function_(&function) {}

    [[nodiscard]] const ControlFlowGraph& cfg();
    [[nodiscard]] const UseDefInfo& use_def();
    [[nodiscard]] const DominatorTree& dominators();

    void invalidate_all();

private:
    const ir::Function* function_;
    bool has_cfg_{};
    bool has_use_def_{};
    bool has_dominators_{};
    ControlFlowGraph cfg_;
    UseDefInfo use_def_;
    DominatorTree dominators_;
};

} // namespace forge::analysis
