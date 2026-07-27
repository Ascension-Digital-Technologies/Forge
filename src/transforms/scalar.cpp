#include "forge/transforms/scalar.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace forge::transforms {
namespace {
std::optional<long long> number(const std::string& text) {
    long long value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error == std::errc{} && end == text.data() + text.size()) return value;
    return {};
}

}

pass::PassResult ConstantFoldPass::run(ir::Function& function,
                                       analysis::FunctionAnalysisManager& analyses) {
    struct ConstantDefinition {
        long long value{};
        std::string block;
        std::size_t operation_index{};
    };
    pass::PassResult result;
    std::unordered_map<std::string, ConstantDefinition> constants;
    const auto& dominators = analyses.dominators();
    for (auto& block : function.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            auto& operation = block.operations[index];
            if (operation.opcode == "const" && !operation.result.empty() && operation.operands.size() == 1) {
                if (auto value = number(operation.operands[0]))
                    constants[operation.result] = {*value, block.name, index};
                continue;
            }
            if ((operation.opcode == "add" || operation.opcode == "sub" ||
                 operation.opcode == "mul" || operation.opcode == "div") &&
                !operation.result.empty() && operation.operands.size() == 2) {
                const auto available = [&](const std::string& name) -> std::optional<long long> {
                    const auto found = constants.find(name);
                    if (found == constants.end()) return {};
                    const auto& definition = found->second;
                    const bool dominates = definition.block == block.name
                        ? definition.operation_index < index
                        : dominators.dominates(definition.block, block.name);
                    if (!dominates) return {};
                    return definition.value;
                };
                const auto left = available(operation.operands[0]);
                const auto right = available(operation.operands[1]);
                if (!left || !right) continue;
                if (operation.opcode == "div" && *right == 0) continue;
                long long value{};
                if (operation.opcode == "add") value = *left + *right;
                else if (operation.opcode == "sub") value = *left - *right;
                else if (operation.opcode == "mul") value = *left * *right;
                else value = *left / *right;
                operation.opcode = "const";
                operation.operands = {std::to_string(value)};
                constants[operation.result] = {value, block.name, index};
                result.changed = true;
                ++result.operations_rewritten;
            }
        }
    }
    return result;
}

pass::PassResult CopyPropagationPass::run(ir::Function& function,
                                          analysis::FunctionAnalysisManager& analyses) {
    struct CopyDefinition {
        std::string source;
        std::string block;
        std::size_t operation_index{};
        std::size_t replaced_uses{};
    };

    pass::PassResult result;
    std::unordered_map<std::string, CopyDefinition> copies;
    for (const auto& block : function.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            const auto& operation = block.operations[index];
            if (operation.opcode == "copy" && !operation.result.empty() &&
                operation.operands.size() == 1) {
                copies.emplace(operation.result,
                               CopyDefinition{operation.operands.front(), block.name, index, 0});
            }
        }
    }
    if (copies.empty()) return result;

    const auto& dominators = analyses.dominators();
    for (auto& block : function.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            auto& operation = block.operations[index];
            const auto rewrite = [&](std::string& value) {
                const auto iterator = copies.find(value);
                if (iterator == copies.end()) return;
                const auto& copy = iterator->second;
                const bool available = copy.block == block.name
                    ? copy.operation_index < index
                    : dominators.dominates(copy.block, block.name);
                if (!available) return;
                value = copy.source;
                ++copies.at(iterator->first).replaced_uses;
            };
            for (auto& operand : operation.operands) rewrite(operand);
            for (auto& arguments : operation.successor_arguments)
                for (auto& argument : arguments) rewrite(argument);
        }
    }

    const auto uses = analysis::build_use_def(function);
    for (auto& block : function.blocks) {
        const auto before = block.operations.size();
        block.operations.erase(std::remove_if(block.operations.begin(), block.operations.end(),
            [&](const ir::Operation& operation) {
                const auto iterator = copies.find(operation.result);
                if (operation.opcode != "copy" || iterator == copies.end()) return false;
                const auto use_iterator = uses.use_count.find(operation.result);
                const std::size_t original_uses = use_iterator == uses.use_count.end()
                    ? 0 : use_iterator->second;
                return iterator->second.replaced_uses == original_uses;
            }), block.operations.end());
        result.operations_removed += before - block.operations.size();
    }
    result.changed = result.operations_removed != 0;
    return result;
}

pass::PassResult BranchFoldPass::run(ir::Function& function,
                                     analysis::FunctionAnalysisManager& analyses) {
    struct ConstantDefinition {
        long long value{};
        std::string block;
        std::size_t operation_index{};
    };

    pass::PassResult result;
    std::unordered_map<std::string, ConstantDefinition> constants;
    for (const auto& block : function.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            const auto& operation = block.operations[index];
            if (operation.opcode == "const" && operation.operands.size() == 1 &&
                !operation.result.empty()) {
                if (auto value = number(operation.operands.front()))
                    constants[operation.result] = {*value, block.name, index};
            }
        }
    }

    const auto& dominators = analyses.dominators();
    for (auto& block : function.blocks) {
        if (block.operations.empty()) continue;
        auto& terminator = block.operations.back();
        if (terminator.opcode != "branch" || terminator.operands.size() != 1 ||
            terminator.successors.size() != 2 || terminator.successor_arguments.size() != 2)
            continue;
        const auto condition = constants.find(terminator.operands.front());
        if (condition == constants.end()) continue;
        const auto terminator_index = block.operations.size() - 1;
        const bool available = condition->second.block == block.name
            ? condition->second.operation_index < terminator_index
            : dominators.dominates(condition->second.block, block.name);
        if (!available) continue;
        const std::size_t selected = condition->second.value != 0 ? 0 : 1;
        terminator.opcode = "jump";
        terminator.operands.clear();
        terminator.successors = {terminator.successors[selected]};
        terminator.successor_arguments = {terminator.successor_arguments[selected]};
        result.changed = true;
        ++result.operations_rewritten;
    }
    return result;
}

pass::PassResult DeadCodeEliminationPass::run(ir::Function& function,
                                              analysis::FunctionAnalysisManager&) {
    pass::PassResult result;
    bool changed = true;
    while (changed) {
        changed = false;
        const auto info = analysis::build_use_def(function);
        for (auto& block : function.blocks) {
            const auto before = block.operations.size();
            block.operations.erase(std::remove_if(block.operations.begin(), block.operations.end(),
                [&](const ir::Operation& operation) {
                    return !operation.result.empty() && !operation.has_side_effects() &&
                           !info.use_count.contains(operation.result);
                }), block.operations.end());
            const auto removed = before - block.operations.size();
            if (removed != 0) {
                changed = true;
                result.changed = true;
                result.operations_removed += removed;
            }
        }
    }
    return result;
}

pass::PassResult SimplifyCFGPass::run(ir::Function& function,
                                      analysis::FunctionAnalysisManager& analyses) {
    pass::PassResult result;
    if (function.blocks.empty()) return result;
    const auto reachable = analyses.cfg().reachable;
    const auto before = function.blocks.size();
    function.blocks.erase(std::remove_if(function.blocks.begin(), function.blocks.end(),
        [&](const ir::Block& block) { return !reachable.contains(block.name); }),
        function.blocks.end());
    result.blocks_removed = before - function.blocks.size();
    result.changed = result.blocks_removed != 0;
    return result;
}


pass::PassResult AlgebraicSimplificationPass::run(ir::Function& function,
                                                   analysis::FunctionAnalysisManager& analyses) {
    struct ConstantDefinition {
        long long value{};
        std::string block;
        std::size_t operation_index{};
    };
    pass::PassResult result;
    std::unordered_map<std::string, ConstantDefinition> constants;
    const auto& dominators = analyses.dominators();
    for (auto& block : function.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            auto& operation = block.operations[index];
            if (operation.opcode == "const" && operation.operands.size() == 1 && !operation.result.empty()) {
                if (auto value = number(operation.operands.front()))
                    constants[operation.result] = {*value, block.name, index};
                continue;
            }
            if (operation.result.empty()) continue;
            const auto constant = [&](std::size_t operand_index) -> std::optional<long long> {
                if (operand_index >= operation.operands.size()) return {};
                const auto found = constants.find(operation.operands[operand_index]);
                if (found == constants.end()) return {};
                const auto& definition = found->second;
                const bool available = definition.block == block.name
                    ? definition.operation_index < index
                    : dominators.dominates(definition.block, block.name);
                return available ? std::optional<long long>{definition.value} : std::optional<long long>{};
            };
            const auto make_copy = [&](const std::string& source) {
                operation.opcode = "copy";
                operation.operands = {source};
                result.changed = true;
                ++result.operations_rewritten;
            };
            const auto make_zero = [&] {
                operation.opcode = "const";
                operation.operands = {"0"};
                constants[operation.result] = {0, block.name, index};
                result.changed = true;
                ++result.operations_rewritten;
            };
            if (operation.operands.size() == 2) {
                const auto left = constant(0);
                const auto right = constant(1);
                if ((operation.opcode == "add" || operation.opcode == "or" || operation.opcode == "xor") && right == 0)
                    make_copy(operation.operands[0]);
                else if ((operation.opcode == "add" || operation.opcode == "or" || operation.opcode == "xor") && left == 0)
                    make_copy(operation.operands[1]);
                else if (operation.opcode == "sub" && right == 0)
                    make_copy(operation.operands[0]);
                else if (operation.opcode == "mul" && right == 1)
                    make_copy(operation.operands[0]);
                else if (operation.opcode == "mul" && left == 1)
                    make_copy(operation.operands[1]);
                else if ((operation.opcode == "mul" || operation.opcode == "and") && (left == 0 || right == 0))
                    make_zero();
                else if ((operation.opcode == "shl" || operation.opcode == "shr.signed" || operation.opcode == "shr.unsigned") && right == 0)
                    make_copy(operation.operands[0]);
            }
        }
    }
    return result;
}

pass::PassResult CommonSubexpressionEliminationPass::run(ir::Function& function,
                                                          analysis::FunctionAnalysisManager& analyses) {
    struct Expression {
        std::string opcode;
        ir::Type type;
        std::vector<std::string> operands;
        std::string value;
        std::string block;
        std::size_t operation_index{};
    };
    pass::PassResult result;
    std::vector<Expression> available;
    const auto& dominators = analyses.dominators();
    const auto is_candidate = [](const ir::Operation& operation) {
        static const std::unordered_set<std::string> pure{
            "const", "add", "sub", "mul", "div", "div.signed", "div.unsigned",
            "rem.signed", "rem.unsigned", "and", "or", "xor", "shl", "shr.signed",
            "shr.unsigned", "neg", "not", "cmp.eq", "cmp.ne", "cmp.lt", "cmp.le",
            "cmp.gt", "cmp.ge", "cmp.ult", "cmp.ule", "cmp.ugt", "cmp.uge",
            "truncate", "zero_extend", "sign_extend", "bitcast", "func.address", "callback.address"};
        return !operation.result.empty() && pure.contains(operation.opcode);
    };
    for (auto& block : function.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            auto& operation = block.operations[index];
            if (!is_candidate(operation)) continue;
            const auto match = std::find_if(available.rbegin(), available.rend(), [&](const Expression& expression) {
                if (expression.opcode != operation.opcode || expression.type != operation.type || expression.operands != operation.operands)
                    return false;
                return expression.block == block.name ? expression.operation_index < index
                                                      : dominators.dominates(expression.block, block.name);
            });
            if (match != available.rend()) {
                operation.opcode = "copy";
                operation.operands = {match->value};
                result.changed = true;
                ++result.operations_rewritten;
            } else {
                available.push_back({operation.opcode, operation.type, operation.operands,
                                     operation.result, block.name, index});
            }
        }
    }
    return result;
}

pass::PassResult SparseConditionalConstantPropagationPass::run(
    ir::Function& function, analysis::FunctionAnalysisManager& analyses) {
    pass::PassResult total;
    bool changed = true;
    unsigned iterations = 0;
    while (changed && iterations++ < 16) {
        changed = false;
        ConstantFoldPass fold;
        auto folded = fold.run(function, analyses);
        total += folded;
        changed = changed || folded.changed;
        if (folded.changed) analyses.invalidate_all();
        BranchFoldPass branch;
        auto branched = branch.run(function, analyses);
        total += branched;
        changed = changed || branched.changed;
        if (branched.changed) analyses.invalidate_all();
        SimplifyCFGPass cfg;
        auto simplified = cfg.run(function, analyses);
        total += simplified;
        changed = changed || simplified.changed;
        if (simplified.changed) analyses.invalidate_all();
        CopyPropagationPass copies;
        auto propagated = copies.run(function, analyses);
        total += propagated;
        changed = changed || propagated.changed;
        if (propagated.changed) analyses.invalidate_all();
    }
    total.changed = total.changed || changed;
    return total;
}

} // namespace forge::transforms
