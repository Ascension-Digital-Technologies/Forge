#include "forge-c/forge.h"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
int fail(const char* message) {
    std::cerr << message << ": " << forge_last_error() << '\n';
    return 1;
}
}

int main() {
    if (FORGE_C_API_VERSION != 9) return fail("unexpected C API version");
    auto* context = forge_context_create();
    auto* module = forge_module_create(context, "c_api_test");
    const forge_type_kind_t parameters[] = {FORGE_TYPE_I64, FORGE_TYPE_I64};
    auto* add = forge_function_create(module, "add", FORGE_TYPE_I64, parameters, 2);
    auto* entry = forge_block_create(add, "entry");

    // Create another function after the first handles to prove index-backed handles survive vector growth.
    auto* caller = forge_function_create(module, "caller", FORGE_TYPE_I64, nullptr, 0);
    auto* caller_entry = forge_block_create(caller, "entry");
    auto* builder = forge_builder_create(context, module);

    forge_builder_position_at_end(builder, entry);
    forge_builder_set_source_range(builder, "sample.c", 10, 2, 10, 14);
    forge_module_set_metadata(module, "frontend.language", "C");
    forge_builder_set_next_attribute(builder, "frontend.ast_id", "7");
    const char* lhs = forge_function_parameter(add, 0);
    const char* rhs = forge_function_parameter(add, 1);
    const char* sum = forge_builder_binary(builder, FORGE_OPCODE_ADD, FORGE_TYPE_I64, lhs, rhs);
    if (sum == nullptr) return fail("failed to build add");
    if (std::strcmp(forge_module_get_metadata(module, "frontend.language"), "C") != 0)
        return fail("module metadata mismatch");
    forge_builder_return(builder, sum);
    if (!forge_builder_has_insertion_point(builder) || !forge_builder_insertion_block_terminated(builder))
        return fail("builder state query failed");

    forge_builder_position_at_end(builder, caller_entry);
    const char* left = forge_builder_constant(builder, FORGE_TYPE_I64, "20");
    const std::string left_copy = left ? left : "";
    const char* right = forge_builder_constant(builder, FORGE_TYPE_I64, "22");
    const std::string right_copy = right ? right : "";
    const char* arguments[] = {left_copy.c_str(), right_copy.c_str()};
    const char* result = forge_builder_call(builder, FORGE_TYPE_I64, "add", arguments, 2);
    if (result == nullptr) return fail("failed to build call");
    forge_builder_return(builder, result);

    char error[256]{};
    if (forge_module_verify(module, error, sizeof(error)) != 1) {
        std::cerr << "verification failed: " << error << '\n';
        return 1;
    }
    const size_t required = forge_module_print(module, nullptr, 0);
    if (required < 2) return fail("failed to size printed module");
    std::vector<char> text(required);
    if (forge_module_print(module, text.data(), text.size()) != required) return fail("failed to print module");
    if (std::strstr(text.data(), "call i64 @add") == nullptr) return fail("printed module omitted call");
    const size_t map_required = forge_module_source_map_json(module, nullptr, 0);
    if (map_required < 2) return fail("failed to size source map");
    std::vector<char> source_map(map_required);
    if (forge_module_source_map_json(module, source_map.data(), source_map.size()) != map_required)
        return fail("failed to generate source map");
    if (std::strstr(source_map.data(), "frontend.ast_id") == nullptr ||
        std::strstr(source_map.data(), "sample.c") == nullptr)
        return fail("source map omitted frontend metadata");

    const size_t semantic_required = forge_module_semantic_fingerprint(module, nullptr, 0);
    const size_t frontend_required = forge_module_frontend_fingerprint(module, nullptr, 0);
    if (semantic_required != 65 || frontend_required != 65) return fail("fingerprint size mismatch");
    std::vector<char> semantic(semantic_required);
    std::vector<char> frontend(frontend_required);
    forge_module_semantic_fingerprint(module, semantic.data(), semantic.size());
    forge_module_frontend_fingerprint(module, frontend.data(), frontend.size());
    if (std::strlen(semantic.data()) != 64 || std::strlen(frontend.data()) != 64)
        return fail("fingerprint output mismatch");
    const size_t manifest_required = forge_module_incremental_manifest_json(module, nullptr, 0);
    std::vector<char> manifest(manifest_required);
    forge_module_incremental_manifest_json(module, manifest.data(), manifest.size());
    if (std::strstr(manifest.data(), "semanticFingerprint") == nullptr ||
        std::strstr(manifest.data(), "caller") == nullptr)
        return fail("incremental manifest missing data");
    const size_t cache_required = forge_module_cache_key(module, "c-test", "-O2", nullptr, 0);
    if (cache_required != 65) return fail("cache key size mismatch");
    std::vector<char> cache_key(cache_required);
    forge_module_cache_key(module, "c-test", "-O2", cache_key.data(), cache_key.size());
    if (std::strlen(cache_key.data()) != 64) return fail("cache key output mismatch");

    auto* invalid = forge_function_create(module, "invalid", FORGE_TYPE_I64, nullptr, 0);
    auto* invalid_entry = forge_block_create(invalid, "entry");
    (void)invalid_entry;
    if (forge_module_verify(module, error, sizeof(error)) != 0) return fail("invalid module unexpectedly verified");
    if (forge_module_diagnostic_count(module) == 0) return fail("structured diagnostics missing");
    if (forge_module_diagnostic_severity(module, 0) != FORGE_DIAGNOSTIC_ERROR)
        return fail("diagnostic severity mismatch");
    const char* diagnostic = forge_module_diagnostic_message(module, 0);
    if (diagnostic == nullptr || std::strlen(diagnostic) == 0) return fail("diagnostic message missing");
    (void)forge_module_diagnostic_file(module, 0);
    (void)forge_module_diagnostic_line(module, 0);
    (void)forge_module_diagnostic_column(module, 0);
    (void)forge_module_diagnostic_end_line(module, 0);
    (void)forge_module_diagnostic_end_column(module, 0);
    forge_block_destroy(invalid_entry);
    forge_function_destroy(invalid);

    forge_module_t* current_module = forge_module_create(context, "current");
    if (current_module == nullptr) return fail("failed to create current module");
    forge_function_t* current_function = forge_function_create(current_module, "changed", FORGE_TYPE_VOID, nullptr, 0);
    forge_block_t* current_block = forge_block_create(current_function, "entry");
    forge_builder_t* current_builder = forge_builder_create(context, current_module);
    forge_builder_position_at_end(current_builder, current_block);
    forge_builder_return(current_builder, nullptr);
    const size_t plan_required = forge_module_incremental_build_plan_json(
        module, current_module, "c-test", "-O2;x86_64", nullptr, 0);
    if (plan_required == 0) return fail("incremental build plan unavailable");
    std::vector<char> plan(plan_required);
    forge_module_incremental_build_plan_json(module, current_module, "c-test", "-O2;x86_64", plan.data(), plan.size());
    if (std::string(plan.data()).find("\"rebuild\"") == std::string::npos)
        return fail("incremental build plan missing summary");
    const size_t schedule_required = forge_module_parallel_build_schedule_json(
        module, current_module, "c-test", "-O2;x86_64", 2, nullptr, 0);
    if (schedule_required == 0) return fail("parallel build schedule unavailable");
    std::vector<char> schedule(schedule_required);
    forge_module_parallel_build_schedule_json(
        module, current_module, "c-test", "-O2;x86_64", 2, schedule.data(), schedule.size());
    if (std::string(schedule.data()).find("\"requestedWorkers\":2") == std::string::npos)
        return fail("parallel build schedule missing workers");
    forge_builder_destroy(current_builder);
    forge_block_destroy(current_block);
    forge_function_destroy(current_function);
    forge_module_destroy(current_module);

    const size_t dependency_schedule_required = forge_module_dependency_build_schedule_json(
        module, module, "c", "-O2", 2, nullptr, 0);
    if (dependency_schedule_required == 0) return fail("dependency schedule size failed");
    std::vector<char> dependency_schedule(dependency_schedule_required);
    forge_module_dependency_build_schedule_json(
        module, module, "c", "-O2", 2, dependency_schedule.data(), dependency_schedule.size());
    if (std::strstr(dependency_schedule.data(), "\"levels\"") == nullptr)
        return fail("dependency schedule JSON mismatch");

    forge_builder_destroy(builder);
    forge_block_destroy(caller_entry);
    forge_function_destroy(caller);
    forge_block_destroy(entry);
    forge_function_destroy(add);
    forge_module_destroy(module);
    forge_context_destroy(context);
    std::cout << "C frontend API v8 parallel-build test passed\n";
    return 0;
}
