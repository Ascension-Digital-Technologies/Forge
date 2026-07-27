#pragma once
#include <string>
#include "forge/ir/module.hpp"

namespace forge::ir {
[[nodiscard]] std::string build_source_map_json(const Module& module);
}
