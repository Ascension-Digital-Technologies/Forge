#include "forge/analysis/function_analysis.hpp"

#include <algorithm>
#include <queue>

namespace forge::analysis {
namespace {
void add_use(UseDefInfo& info, const std::string& value) {
    if (value.starts_with('%')) ++info.use_count[value];
}
}

ControlFlowGraph build_cfg(const ir::Function& function) {
    ControlFlowGraph cfg;
    for (const auto& block : function.blocks) {
        auto& successors = cfg.successors[block.name];
        cfg.predecessors.try_emplace(block.name);
        if (!block.operations.empty()) {
            for (const auto& successor : block.operations.back().successors) {
                if (std::find(successors.begin(), successors.end(), successor) == successors.end())
                    successors.push_back(successor);
                cfg.predecessors[successor].push_back(block.name);
            }
        }
    }

    if (!function.blocks.empty()) {
        std::queue<std::string> pending;
        pending.push(function.blocks.front().name);
        cfg.reachable.insert(function.blocks.front().name);
        while (!pending.empty()) {
            auto block = std::move(pending.front());
            pending.pop();
            for (const auto& successor : cfg.successors[block]) {
                if (cfg.reachable.insert(successor).second) pending.push(successor);
            }
        }
    }
    return cfg;
}

UseDefInfo build_use_def(const ir::Function& function) {
    UseDefInfo info;
    for (const auto& parameter : function.parameters)
        info.definitions.emplace(parameter.name, UseDefInfo::Definition{"", 0, true});

    for (const auto& block : function.blocks) {
        for (const auto& parameter : block.parameters)
            info.definitions.emplace(parameter.name, UseDefInfo::Definition{block.name, 0, true});
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            const auto& operation = block.operations[index];
            if (!operation.result.empty())
                info.definitions.emplace(operation.result,
                                         UseDefInfo::Definition{block.name, index, false});
            for (const auto& operand : operation.operands) add_use(info, operand);
            for (const auto& arguments : operation.successor_arguments)
                for (const auto& argument : arguments) add_use(info, argument);
        }
    }
    return info;
}

DominatorTree build_dominator_tree(const ir::Function& function,
                                   const ControlFlowGraph& cfg) {
    DominatorTree tree;
    if (function.blocks.empty()) return tree;

    std::unordered_set<std::string> all_reachable = cfg.reachable;
    const auto& entry = function.blocks.front().name;
    for (const auto& block : function.blocks) {
        if (!cfg.reachable.contains(block.name)) continue;
        tree.dominators[block.name] = block.name == entry
            ? std::unordered_set<std::string>{entry}
            : all_reachable;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& block : function.blocks) {
            if (block.name == entry || !cfg.reachable.contains(block.name)) continue;
            std::unordered_set<std::string> next;
            const auto pred_it = cfg.predecessors.find(block.name);
            bool first = true;
            if (pred_it != cfg.predecessors.end()) {
                for (const auto& predecessor : pred_it->second) {
                    if (!cfg.reachable.contains(predecessor)) continue;
                    if (first) {
                        next = tree.dominators[predecessor];
                        first = false;
                    } else {
                        std::unordered_set<std::string> intersection;
                        for (const auto& candidate : next)
                            if (tree.dominators[predecessor].contains(candidate))
                                intersection.insert(candidate);
                        next = std::move(intersection);
                    }
                }
            }
            next.insert(block.name);
            if (next != tree.dominators[block.name]) {
                tree.dominators[block.name] = std::move(next);
                changed = true;
            }
        }
    }
    return tree;
}

bool DominatorTree::dominates(const std::string& candidate,
                              const std::string& block) const {
    const auto iterator = dominators.find(block);
    return iterator != dominators.end() && iterator->second.contains(candidate);
}

const ControlFlowGraph& FunctionAnalysisManager::cfg() {
    if (!has_cfg_) {
        cfg_ = build_cfg(*function_);
        has_cfg_ = true;
    }
    return cfg_;
}

const UseDefInfo& FunctionAnalysisManager::use_def() {
    if (!has_use_def_) {
        use_def_ = build_use_def(*function_);
        has_use_def_ = true;
    }
    return use_def_;
}

const DominatorTree& FunctionAnalysisManager::dominators() {
    if (!has_dominators_) {
        dominators_ = build_dominator_tree(*function_, cfg());
        has_dominators_ = true;
    }
    return dominators_;
}

void FunctionAnalysisManager::invalidate_all() {
    has_cfg_ = false;
    has_use_def_ = false;
    has_dominators_ = false;
    cfg_ = {};
    use_def_ = {};
    dominators_ = {};
}

} // namespace forge::analysis
