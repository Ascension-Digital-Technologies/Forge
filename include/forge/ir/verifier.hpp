#pragma once
#include "forge/diagnostics/diagnostic.hpp"
#include "forge/ir/module.hpp"

namespace forge::ir {
[[nodiscard]] Diagnostics verify_module(const Module& module);
}
