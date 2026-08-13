// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include "forge/diagnostics/diagnostic.hpp"
#include "forge/ir/module.hpp"
#include "forge/machine/module.hpp"
#include "forge/machine/optimize.hpp"

namespace forge::machine {

struct LowerResult {
    std::optional<Module> module;
    Diagnostics diagnostics;
    [[nodiscard]] bool ok() const noexcept { return module.has_value() && diagnostics.empty(); }
};

struct LowerOptions {
    SlpCostModel slp_cost_model{SlpCostModel::x86_64_sse2()};
};

// Initial backend slice: straight-line i32 functions with i32 parameters,
// constants, copies, add/sub/mul, and one return operation.
[[nodiscard]] LowerResult lower_module(const ir::Module& module, const LowerOptions& options = {});

} // namespace forge::machine
