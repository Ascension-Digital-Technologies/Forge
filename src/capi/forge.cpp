#include "forge-c/forge.h"
#include "forge/ir/builder.hpp"
#include "forge/ir/incremental.hpp"
#include "forge/ir/artifact_cache.hpp"
#include "forge/ir/build_driver.hpp"
#include "forge/ir/dependency_build.hpp"
#include "forge/ir/printer.hpp"
#include "forge/ir/source_map.hpp"
#include "forge/ir/verifier.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

struct forge_context { forge::ir::Context value; };
struct forge_module { forge::ir::Module* value{}; mutable forge::Diagnostics diagnostics; mutable std::string scratch; };
struct forge_function { forge_module* module{}; size_t index{}; std::vector<std::string> parameter_names; };
struct forge_block { forge_function* function{}; size_t index{}; std::vector<std::string> parameter_names; };
struct forge_builder { std::unique_ptr<forge::ir::IRBuilder> value; std::string result; };

namespace {
thread_local std::string last_error;
void set_error(const char* message) { last_error = message ? message : "unknown Forge C API error"; }
forge::ir::Type type_of(forge_type_kind_t kind) {
    using forge::ir::TypeKind;
    switch (kind) {
    case FORGE_TYPE_VOID: return forge::ir::Type(TypeKind::void_);
    case FORGE_TYPE_I1: return forge::ir::Type(TypeKind::i1);
    case FORGE_TYPE_I8: return forge::ir::Type(TypeKind::i8);
    case FORGE_TYPE_I16: return forge::ir::Type(TypeKind::i16);
    case FORGE_TYPE_I32: return forge::ir::Type(TypeKind::i32);
    case FORGE_TYPE_I64: return forge::ir::Type(TypeKind::i64);
    case FORGE_TYPE_F32: return forge::ir::Type(TypeKind::f32);
    case FORGE_TYPE_F64: return forge::ir::Type(TypeKind::f64);
    case FORGE_TYPE_PTR: return forge::ir::Type(TypeKind::ptr);
    }
    return forge::ir::Type(TypeKind::void_);
}
forge::ir::Opcode opcode_of(forge_opcode_t opcode) {
    using forge::ir::Opcode;
    switch (opcode) {
    case FORGE_OPCODE_ADD: return Opcode::add;
    case FORGE_OPCODE_SUBTRACT: return Opcode::subtract;
    case FORGE_OPCODE_MULTIPLY: return Opcode::multiply;
    case FORGE_OPCODE_DIVIDE_SIGNED: return Opcode::divide_signed;
    case FORGE_OPCODE_DIVIDE_UNSIGNED: return Opcode::divide_unsigned;
    case FORGE_OPCODE_REMAINDER_SIGNED: return Opcode::remainder_signed;
    case FORGE_OPCODE_REMAINDER_UNSIGNED: return Opcode::remainder_unsigned;
    case FORGE_OPCODE_AND: return Opcode::bit_and;
    case FORGE_OPCODE_OR: return Opcode::bit_or;
    case FORGE_OPCODE_XOR: return Opcode::bit_xor;
    case FORGE_OPCODE_SHIFT_LEFT: return Opcode::shift_left;
    case FORGE_OPCODE_SHIFT_RIGHT_SIGNED: return Opcode::shift_right_signed;
    case FORGE_OPCODE_SHIFT_RIGHT_UNSIGNED: return Opcode::shift_right_unsigned;
    case FORGE_OPCODE_COMPARE_EQUAL: return Opcode::compare_equal;
    case FORGE_OPCODE_COMPARE_NOT_EQUAL: return Opcode::compare_not_equal;
    case FORGE_OPCODE_COMPARE_LESS_SIGNED: return Opcode::compare_less_signed;
    case FORGE_OPCODE_COMPARE_LESS_UNSIGNED: return Opcode::compare_less_unsigned;
    case FORGE_OPCODE_COMPARE_LESS_EQUAL_SIGNED: return Opcode::compare_less_equal_signed;
    case FORGE_OPCODE_COMPARE_LESS_EQUAL_UNSIGNED: return Opcode::compare_less_equal_unsigned;
    }
    return Opcode::add;
}
forge::ir::Function* resolve(forge_function* handle) {
    if (handle == nullptr || handle->module == nullptr || handle->module->value == nullptr ||
        handle->index >= handle->module->value->functions().size()) return nullptr;
    return &handle->module->value->functions()[handle->index];
}
forge::ir::Block* resolve(forge_block* handle) {
    auto* function = handle ? resolve(handle->function) : nullptr;
    if (function == nullptr || handle->index >= function->blocks.size()) return nullptr;
    return &function->blocks[handle->index];
}
const forge::ir::Block* resolve(const forge_block* handle) { return resolve(const_cast<forge_block*>(handle)); }
template <typename F> const char* result_call(forge_builder* builder, F&& action) {
    if (builder == nullptr || builder->value == nullptr) { set_error("builder is null"); return nullptr; }
    try { builder->result = action(); last_error.clear(); return builder->result.c_str(); }
    catch (const std::exception& error) { last_error = error.what(); return nullptr; }
}
std::vector<std::string> string_args(const char* const* arguments, size_t count) {
    std::vector<std::string> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) result.emplace_back(arguments && arguments[i] ? arguments[i] : "");
    return result;
}
}

extern "C" {
forge_context_t* forge_context_create(void) { try { last_error.clear(); return new forge_context; } catch (...) { set_error("failed to create Forge context"); return nullptr; } }
void forge_context_destroy(forge_context_t* context) { delete context; }
forge_module_t* forge_module_create(forge_context_t* context, const char* name) {
    if (context == nullptr) { set_error("context is null"); return nullptr; }
    try { last_error.clear(); return new forge_module{&context->value.create_module(name ? name : "anonymous"), {}, {}}; }
    catch (...) { set_error("failed to create module"); return nullptr; }
}
void forge_module_destroy(forge_module_t* module) { delete module; }
forge_function_t* forge_function_create(forge_module_t* module, const char* name, forge_type_kind_t return_type,
                                        const forge_type_kind_t* parameter_types, size_t parameter_count) {
    if (module == nullptr || module->value == nullptr) { set_error("module is null"); return nullptr; }
    if (parameter_count != 0 && parameter_types == nullptr) { set_error("parameter types are null"); return nullptr; }
    try {
        forge::ir::Function fn; fn.name = name ? name : "anonymous"; fn.return_type = type_of(return_type);
        auto handle = std::make_unique<forge_function>(); handle->module = module; handle->index = module->value->functions().size();
        for (size_t i = 0; i < parameter_count; ++i) {
            auto parameter_name = "%arg" + std::to_string(i);
            fn.parameters.emplace_back(parameter_name, type_of(parameter_types[i]));
            handle->parameter_names.push_back(std::move(parameter_name));
        }
        module->value->functions().push_back(std::move(fn)); last_error.clear(); return handle.release();
    } catch (...) { set_error("failed to create function"); return nullptr; }
}
void forge_function_destroy(forge_function_t* function) { delete function; }
forge_block_t* forge_block_create_with_parameters(forge_function_t* function, const char* name,
                                                   const forge_type_kind_t* parameter_types, size_t parameter_count) {
    auto* fn = resolve(function);
    if (fn == nullptr) { set_error("function is invalid"); return nullptr; }
    if (parameter_count != 0 && parameter_types == nullptr) { set_error("block parameter types are null"); return nullptr; }
    try {
        forge::ir::Block block; block.name = name ? name : "entry";
        auto handle = std::make_unique<forge_block>(); handle->function = function; handle->index = fn->blocks.size();
        for (size_t i = 0; i < parameter_count; ++i) {
            auto parameter_name = "%block_arg" + std::to_string(i);
            block.parameters.emplace_back(parameter_name, type_of(parameter_types[i]));
            handle->parameter_names.push_back(std::move(parameter_name));
        }
        fn->blocks.push_back(std::move(block)); last_error.clear(); return handle.release();
    } catch (...) { set_error("failed to create block"); return nullptr; }
}
forge_block_t* forge_block_create(forge_function_t* function, const char* name) { return forge_block_create_with_parameters(function, name, nullptr, 0); }
void forge_block_destroy(forge_block_t* block) { delete block; }
forge_builder_t* forge_builder_create(forge_context_t* context, forge_module_t* module) {
    if (context == nullptr || module == nullptr || module->value == nullptr) { set_error("context or module is null"); return nullptr; }
    try { last_error.clear(); return new forge_builder{std::make_unique<forge::ir::IRBuilder>(context->value, *module->value), {}}; }
    catch (...) { set_error("failed to create builder"); return nullptr; }
}
void forge_builder_destroy(forge_builder_t* builder) { delete builder; }
void forge_builder_position_at_end(forge_builder_t* builder, forge_block_t* block) {
    auto* resolved = resolve(block); if (builder == nullptr || builder->value == nullptr || resolved == nullptr) { set_error("builder or block is invalid"); return; }
    builder->value->position_at_end(*resolved); last_error.clear();
}
void forge_builder_clear_insertion_point(forge_builder_t* builder) { if (builder && builder->value) builder->value->clear_insertion_point(); }
void forge_builder_set_source_location(forge_builder_t* builder, const char* file, uint32_t line, uint32_t column) { if (builder && builder->value) builder->value->set_source_location({file ? file : "", line, column, line, column}); else set_error("builder is null"); }
void forge_builder_set_source_range(forge_builder_t* builder, const char* file, uint32_t line, uint32_t column, uint32_t end_line, uint32_t end_column) { if (builder && builder->value) builder->value->set_source_range(file ? file : "", line, column, end_line, end_column); else set_error("builder is null"); }
void forge_builder_clear_source_location(forge_builder_t* builder) { if (builder && builder->value) builder->value->clear_source_location(); }
void forge_module_set_metadata(forge_module_t* module, const char* name, const char* value) {
    if (!module || !module->value) { set_error("module is null"); return; }
    try {
        auto& metadata = module->value->metadata();
        const std::string key = name ? name : "";
        if (key.empty()) { set_error("metadata name must not be empty"); return; }
        const auto found = std::find_if(metadata.begin(), metadata.end(), [&](const forge::ir::Attribute& attribute) { return attribute.name == key; });
        if (found != metadata.end()) found->value = value ? value : "";
        else metadata.push_back({key, value ? value : ""});
        last_error.clear();
    } catch (const std::exception& error) { last_error = error.what(); }
}
const char* forge_module_get_metadata(const forge_module_t* module, const char* name) {
    if (!module || !module->value || !name) { set_error("module or metadata name is null"); return nullptr; }
    const auto& metadata = module->value->metadata();
    const auto found = std::find_if(metadata.begin(), metadata.end(), [&](const forge::ir::Attribute& attribute) { return attribute.name == name; });
    if (found == metadata.end()) { set_error("metadata key not found"); return nullptr; }
    last_error.clear();
    return found->value.c_str();
}
void forge_builder_set_next_attribute(forge_builder_t* builder, const char* name, const char* value) {
    if (!builder || !builder->value) { set_error("builder is null"); return; }
    try { builder->value->set_next_attribute(name ? name : "", value ? value : ""); last_error.clear(); }
    catch (const std::exception& error) { last_error = error.what(); }
}
void forge_builder_clear_next_attributes(forge_builder_t* builder) {
    if (!builder || !builder->value) { set_error("builder is null"); return; }
    builder->value->clear_next_attributes();
    last_error.clear();
}
const char* forge_function_parameter(const forge_function_t* function, size_t index) { if (function == nullptr || index >= function->parameter_names.size()) { set_error("parameter index out of range"); return nullptr; } return function->parameter_names[index].c_str(); }
const char* forge_block_parameter(const forge_block_t* block, size_t index) { if (block == nullptr || index >= block->parameter_names.size()) { set_error("block parameter index out of range"); return nullptr; } return block->parameter_names[index].c_str(); }
const char* forge_builder_constant(forge_builder_t* b, forge_type_kind_t t, const char* literal) { return result_call(b, [&]{ return b->value->create_constant(type_of(t), literal ? literal : "0"); }); }
const char* forge_builder_binary(forge_builder_t* b, forge_opcode_t op, forge_type_kind_t t, const char* lhs, const char* rhs) { return result_call(b, [&]{ return b->value->create_binary(opcode_of(op), type_of(t), lhs ? lhs : "", rhs ? rhs : ""); }); }
const char* forge_builder_compare(forge_builder_t* b, forge_opcode_t op, forge_type_kind_t t, const char* lhs, const char* rhs) { return result_call(b, [&]{ return b->value->create_compare(opcode_of(op), type_of(t), lhs ? lhs : "", rhs ? rhs : ""); }); }
const char* forge_builder_stack_alloc(forge_builder_t* b, uint64_t size, uint32_t alignment) { return result_call(b, [&]{ return b->value->create_stack_allocation(size, alignment); }); }
const char* forge_builder_load(forge_builder_t* b, forge_type_kind_t t, const char* address, uint32_t alignment) { return result_call(b, [&]{ return b->value->create_load(type_of(t), address ? address : "", alignment); }); }
void forge_builder_store(forge_builder_t* b, forge_type_kind_t t, const char* value, const char* address, uint32_t alignment) { if (!b || !b->value) { set_error("builder is null"); return; } try { b->value->create_store(type_of(t), value ? value : "", address ? address : "", alignment); last_error.clear(); } catch (const std::exception& e) { last_error=e.what(); } }
const char* forge_builder_call(forge_builder_t* b, forge_type_kind_t t, const char* callee, const char* const* args, size_t count) { return result_call(b, [&]{ return b->value->create_call(type_of(t), callee ? callee : "", string_args(args,count)); }); }
void forge_builder_jump(forge_builder_t* b, const forge_block_t* destination, const char* const* args, size_t count) { auto* d=resolve(destination); if (!b || !b->value || !d) { set_error("builder or destination block is invalid"); return; } try { b->value->create_jump(d->name,string_args(args,count)); last_error.clear(); } catch(const std::exception& e){last_error=e.what();} }
void forge_builder_branch(forge_builder_t* b, const char* condition, const forge_block_t* td, const forge_block_t* fd) { auto* t=resolve(td); auto* f=resolve(fd); if(!b||!b->value||!t||!f){set_error("builder or branch destination is invalid");return;} try{b->value->create_branch(condition?condition:"",t->name,f->name);last_error.clear();}catch(const std::exception&e){last_error=e.what();} }
void forge_builder_return(forge_builder_t* b, const char* value) { if (!b || !b->value) { set_error("builder is null"); return; } try { b->value->create_return(value ? value : ""); last_error.clear(); } catch (const std::exception& e) { last_error=e.what(); } }
void forge_builder_unreachable(forge_builder_t* b) { if(!b||!b->value){set_error("builder is null");return;} try{b->value->create_unreachable();last_error.clear();}catch(const std::exception&e){last_error=e.what();} }
int forge_module_verify(const forge_module_t* module, char* message, size_t capacity) {
    if (!module || !module->value) { set_error("module is null"); return 0; }
    module->diagnostics = forge::ir::verify_module(*module->value);
    if (module->diagnostics.empty()) { last_error.clear(); return 1; }
    last_error = module->diagnostics.front().message;
    if (message && capacity) {
        const auto n = std::min(capacity - 1, last_error.size());
        std::memcpy(message, last_error.data(), n);
        message[n] = '\0';
    }
    return 0;
}
size_t forge_module_diagnostic_count(const forge_module_t* module) {
    return module ? module->diagnostics.size() : 0;
}
forge_diagnostic_severity_t forge_module_diagnostic_severity(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return FORGE_DIAGNOSTIC_ERROR; }
    switch (module->diagnostics[index].severity) {
    case forge::DiagnosticSeverity::note: return FORGE_DIAGNOSTIC_NOTE;
    case forge::DiagnosticSeverity::warning: return FORGE_DIAGNOSTIC_WARNING;
    case forge::DiagnosticSeverity::error: return FORGE_DIAGNOSTIC_ERROR;
    }
    return FORGE_DIAGNOSTIC_ERROR;
}
const char* forge_module_diagnostic_message(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return nullptr; }
    last_error.clear();
    return module->diagnostics[index].message.c_str();
}
const char* forge_module_diagnostic_file(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return nullptr; }
    last_error.clear();
    return module->diagnostics[index].source_file.c_str();
}
uint32_t forge_module_diagnostic_line(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return 0; }
    return static_cast<uint32_t>(module->diagnostics[index].source_line);
}
uint32_t forge_module_diagnostic_column(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return 0; }
    return static_cast<uint32_t>(module->diagnostics[index].source_column);
}
uint32_t forge_module_diagnostic_end_line(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return 0; }
    return static_cast<uint32_t>(module->diagnostics[index].source_end_line);
}
uint32_t forge_module_diagnostic_end_column(const forge_module_t* module, size_t index) {
    if (!module || index >= module->diagnostics.size()) { set_error("diagnostic index out of range"); return 0; }
    return static_cast<uint32_t>(module->diagnostics[index].source_end_column);
}
int forge_builder_has_insertion_point(const forge_builder_t* builder) {
    return builder && builder->value && builder->value->has_insertion_point() ? 1 : 0;
}
int forge_builder_insertion_block_terminated(const forge_builder_t* builder) {
    return builder && builder->value && builder->value->insertion_block_terminated() ? 1 : 0;
}
size_t forge_module_print(const forge_module_t* module, char* output, size_t capacity) { if(!module||!module->value){set_error("module is null");return 0;} auto text=forge::ir::print_module(*module->value); size_t required=text.size()+1; if(output&&capacity){auto n=std::min(capacity-1,text.size());std::memcpy(output,text.data(),n);output[n]='\0';} last_error.clear(); return required; }
size_t forge_module_source_map_json(const forge_module_t* module, char* output, size_t capacity) {
    if (!module || !module->value) { set_error("module is null"); return 0; }
    const auto text = forge::ir::build_source_map_json(*module->value);
    const size_t required = text.size() + 1;
    if (output && capacity) {
        const auto count = std::min(capacity - 1, text.size());
        std::memcpy(output, text.data(), count);
        output[count] = '\0';
    }
    last_error.clear();
    return required;
}
size_t forge_module_semantic_fingerprint(const forge_module_t* module, char* output, size_t capacity) {
    if (!module || !module->value) { set_error("module is null"); return 0; }
    const auto text = forge::ir::build_incremental_snapshot(*module->value).semantic_fingerprint;
    const size_t required = text.size() + 1;
    if (output && capacity) { const auto count = std::min(capacity - 1, text.size()); std::memcpy(output, text.data(), count); output[count] = '\0'; }
    last_error.clear();
    return required;
}
size_t forge_module_frontend_fingerprint(const forge_module_t* module, char* output, size_t capacity) {
    if (!module || !module->value) { set_error("module is null"); return 0; }
    const auto text = forge::ir::build_incremental_snapshot(*module->value).frontend_fingerprint;
    const size_t required = text.size() + 1;
    if (output && capacity) { const auto count = std::min(capacity - 1, text.size()); std::memcpy(output, text.data(), count); output[count] = '\0'; }
    last_error.clear();
    return required;
}
size_t forge_module_incremental_manifest_json(const forge_module_t* module, char* output, size_t capacity) {
    if (!module || !module->value) { set_error("module is null"); return 0; }
    const auto text = forge::ir::build_incremental_manifest_json(forge::ir::build_incremental_snapshot(*module->value));
    const size_t required = text.size() + 1;
    if (output && capacity) { const auto count = std::min(capacity - 1, text.size()); std::memcpy(output, text.data(), count); output[count] = '\0'; }
    last_error.clear();
    return required;
}
size_t forge_module_cache_key(const forge_module_t* module, const char* frontend_id,
                              const char* configuration, char* output, size_t capacity) {
    if (!module || !module->value) { set_error("module is null"); return 0; }
    const auto text = forge::ir::build_cache_key(*module->value, frontend_id ? frontend_id : "", configuration ? configuration : "");
    const size_t required = text.size() + 1;
    if (output && capacity) { const auto count = std::min(capacity - 1, text.size()); std::memcpy(output, text.data(), count); output[count] = '\0'; }
    last_error.clear();
    return required;
}
size_t forge_module_incremental_build_plan_json(const forge_module_t* previous_module,
                                                const forge_module_t* current_module,
                                                const char* frontend_id,
                                                const char* configuration,
                                                char* output, size_t capacity) {
    if (!previous_module || !previous_module->value || !current_module || !current_module->value) {
        set_error("previous or current module is null");
        return 0;
    }
    const auto previous = forge::ir::build_incremental_snapshot(*previous_module->value);
    const auto current = forge::ir::build_incremental_snapshot(*current_module->value);
    const auto plan = forge::ir::build_incremental_build_plan(
        previous, current, frontend_id ? frontend_id : "", configuration ? configuration : "");
    const auto text = forge::ir::build_incremental_build_plan_json(plan);
    const size_t required = text.size() + 1;
    if (output && capacity) {
        const auto count = std::min(capacity - 1, text.size());
        std::memcpy(output, text.data(), count);
        output[count] = '\0';
    }
    last_error.clear();
    return required;
}

size_t forge_module_parallel_build_schedule_json(const forge_module_t* previous_module,
                                                 const forge_module_t* current_module,
                                                 const char* frontend_id,
                                                 const char* configuration,
                                                 size_t requested_workers,
                                                 char* output, size_t capacity) {
    if (!previous_module || !previous_module->value || !current_module || !current_module->value) {
        set_error("previous or current module is null");
        return 0;
    }
    const auto previous = forge::ir::build_incremental_snapshot(*previous_module->value);
    const auto current = forge::ir::build_incremental_snapshot(*current_module->value);
    const auto plan = forge::ir::build_incremental_build_plan(
        previous, current, frontend_id ? frontend_id : "", configuration ? configuration : "");
    const auto schedule = forge::ir::build_parallel_build_schedule(plan, requested_workers);
    const auto text = forge::ir::build_parallel_build_schedule_json(schedule);
    const size_t required = text.size() + 1;
    if (output && capacity) {
        const auto count = std::min(capacity - 1, text.size());
        std::memcpy(output, text.data(), count);
        output[count] = '\0';
    }
    last_error.clear();
    return required;
}
size_t forge_module_dependency_build_schedule_json(const forge_module_t* previous_module,
                                                   const forge_module_t* current_module,
                                                   const char* frontend_id,
                                                   const char* configuration,
                                                   size_t requested_workers,
                                                   char* output, size_t capacity) {
    if (!previous_module || !previous_module->value || !current_module || !current_module->value) {
        set_error("previous or current module is null");
        return 0;
    }
    const auto previous = forge::ir::build_incremental_snapshot(*previous_module->value);
    const auto current = forge::ir::build_incremental_snapshot(*current_module->value);
    auto plan = forge::ir::build_incremental_build_plan(
        previous, current, frontend_id ? frontend_id : "", configuration ? configuration : "");
    const auto graph = forge::ir::build_function_dependency_graph(*current_module->value);
    plan = forge::ir::propagate_dependency_invalidations(
        plan, graph, frontend_id ? frontend_id : "", configuration ? configuration : "");
    const auto schedule = forge::ir::build_dependency_build_schedule(plan, graph, requested_workers);
    const auto text = forge::ir::build_dependency_build_schedule_json(schedule);
    const size_t required = text.size() + 1;
    if (output && capacity) {
        const auto count = std::min(capacity - 1, text.size());
        std::memcpy(output, text.data(), count);
        output[count] = '\0';
    }
    last_error.clear();
    return required;
}
const char* forge_last_error(void) { return last_error.c_str(); }
void forge_clear_error(void) { last_error.clear(); }
}
