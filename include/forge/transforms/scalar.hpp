// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "forge/pass/pass.hpp"

namespace forge::transforms {
class ConstantFoldPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "constant-fold"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class CopyPropagationPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "copy-propagation"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class BranchFoldPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "branch-fold"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class AlgebraicSimplificationPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "algebraic-simplification"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class CommonSubexpressionEliminationPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "cse"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class SparseConditionalConstantPropagationPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "sccp"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};

class MemoryForwardingPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "memory-forwarding"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class LoopInvariantCodeMotionPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "licm"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class DeadCodeEliminationPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "dce"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
class SimplifyCFGPass final : public pass::FunctionPass {
public:
    std::string name() const override { return "simplify-cfg"; }
    pass::PassResult run(ir::Function&, analysis::FunctionAnalysisManager&) override;
};
} // namespace forge::transforms
