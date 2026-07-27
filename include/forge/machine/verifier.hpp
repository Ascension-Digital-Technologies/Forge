#pragma once
#include "forge/diagnostics/diagnostic.hpp"
#include "forge/machine/module.hpp"

namespace forge::machine {
[[nodiscard]] Diagnostics verify_module(const Module& module);
}
