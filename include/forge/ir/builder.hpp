#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "forge/ir/context.hpp"
#include "forge/ir/opcode.hpp"
#include "forge/diagnostics/diagnostic.hpp"

namespace forge::ir {

struct SourceLocation {
    std::string file;
    std::uint32_t line{};
    std::uint32_t column{};
    std::uint32_t end_line{};
    std::uint32_t end_column{};
};

struct FunctionHandle { std::size_t index{}; };
struct BlockHandle { std::size_t function_index{}; std::size_t block_index{}; };

class IRBuilder {
public:
    IRBuilder(Context&, Module& module) noexcept : module_(&module) {}

    [[nodiscard]] Function& create_function(std::string name, Type return_type,
                                            std::vector<ValueDecl> parameters = {}, bool external = false);
    [[nodiscard]] Block& create_block(Function& function, std::string name,
                                      std::vector<ValueDecl> parameters = {});
    [[nodiscard]] FunctionHandle create_function_handle(std::string name, Type return_type,
                                                        std::vector<ValueDecl> parameters = {}, bool external = false);
    [[nodiscard]] BlockHandle create_block_handle(FunctionHandle function, std::string name,
                                                  std::vector<ValueDecl> parameters = {});
    [[nodiscard]] Function& resolve(FunctionHandle handle);
    [[nodiscard]] const Function& resolve(FunctionHandle handle) const;
    [[nodiscard]] Block& resolve(BlockHandle handle);
    [[nodiscard]] const Block& resolve(BlockHandle handle) const;
    [[nodiscard]] FunctionHandle find_function(std::string_view name) const;
    [[nodiscard]] BlockHandle find_block(FunctionHandle function, std::string_view name) const;
    void position_at_end(Block& block) noexcept { block_ = &block; }
    void position_at_end(BlockHandle block) { block_ = &resolve(block); }
    [[nodiscard]] bool has_insertion_point() const noexcept { return block_ != nullptr; }
    [[nodiscard]] bool insertion_block_terminated() const noexcept;
    [[nodiscard]] Diagnostics verify() const;
    void clear_insertion_point() noexcept { block_ = nullptr; }
    void set_source_location(SourceLocation location) { location_ = std::move(location); }
    void set_source_range(std::string file, std::uint32_t line, std::uint32_t column,
                          std::uint32_t end_line, std::uint32_t end_column) {
        location_ = {std::move(file), line, column, end_line, end_column};
    }
    void clear_source_location() noexcept { location_ = {}; }
    void set_module_metadata(std::string name, std::string value);
    [[nodiscard]] std::string_view module_metadata(std::string_view name) const;
    void set_next_attribute(std::string name, std::string value);
    void clear_next_attributes() noexcept { next_attributes_.clear(); }

    [[nodiscard]] std::string create_constant(Type type, std::string literal);
    [[nodiscard]] std::string create_binary(Opcode opcode, Type type, std::string lhs, std::string rhs);
    [[nodiscard]] std::string create_add(Type type, std::string lhs, std::string rhs) {
        return create_binary(Opcode::add, type, std::move(lhs), std::move(rhs));
    }
    [[nodiscard]] std::string create_compare(Opcode opcode, Type operand_type,
                                             std::string lhs, std::string rhs);
    [[nodiscard]] std::string create_copy(Type type, std::string value);
    [[nodiscard]] std::string create_stack_allocation(std::uint64_t size, std::uint32_t alignment = 0);
    [[nodiscard]] std::string create_load(Type type, std::string address, std::uint32_t alignment = 0);
    void create_store(Type type, std::string value, std::string address, std::uint32_t alignment = 0);
    [[nodiscard]] std::string create_pointer_offset(std::string base, std::string byte_offset);
    [[nodiscard]] std::string create_call(Type return_type, std::string callee,
                                          std::vector<std::string> arguments = {});
    void create_return(std::string value = {});
    void create_jump(std::string successor, std::vector<std::string> arguments = {});
    void create_branch(std::string condition, std::string true_successor,
                       std::string false_successor,
                       std::vector<std::string> true_arguments = {},
                       std::vector<std::string> false_arguments = {});
    void create_unreachable();

    [[nodiscard]] Module& module() noexcept { return *module_; }
    [[nodiscard]] const SourceLocation& source_location() const noexcept { return location_; }

private:
    [[nodiscard]] std::string next_value_name();
    Operation& append(Operation operation);
    Module* module_{};
    Block* block_{};
    SourceLocation location_{};
    std::vector<Attribute> next_attributes_;
    std::uint64_t next_value_{};
};

} // namespace forge::ir
