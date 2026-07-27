#pragma once
#include <string>
#include "forge/ir/module.hpp"

namespace forge::ir {
[[nodiscard]] std::string print_module(const Module& module);
}
