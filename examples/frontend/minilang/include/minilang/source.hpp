#pragma once
#include <cstdint>
#include <string>

namespace minilang {

struct SourceSpan {
    std::string file;
    std::uint32_t line{1};
    std::uint32_t column{1};
    std::uint32_t end_line{1};
    std::uint32_t end_column{1};
};

} // namespace minilang
