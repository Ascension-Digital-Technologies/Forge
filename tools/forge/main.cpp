#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "forge/diagnostics/format.hpp"
#include "forge/ir/parser.hpp"
#include "forge/ir/printer.hpp"
#include "forge/ir/verifier.hpp"
#include "forge/machine/lower.hpp"
#include "forge/object/coff.hpp"
#include "forge/object/elf.hpp"
#include "forge/pass/pipeline.hpp"
#include "forge/version.hpp"

namespace {
void usage() {
    std::cerr << "Forge " << FORGE_VERSION_STRING << "\n"
              << "usage: forge <command> [options]\n\n"
              << "commands:\n"
              << "  version                 print version information\n"
              << "  verify <file.fir>       parse and verify a module\n"
              << "  print <file.fir>        print canonical Forge IR\n"
              << "  opt <file.fir> [-O0|-O1|-O2|-O3|-Os|-Oz] optimize and print Forge IR\n"
              << "  compile <file.fir> [-O0|-O1|-O2|-O3|-Os|-Oz] [--pass-stats] [--format=auto|elf|coff] -o <file> emit x86-64 object\n"
              << "\nExecution remains available through forge-run.\n";
}

bool read_file(const std::string& path, std::string& text) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "error: cannot open " << path << '\n';
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    text = stream.str();
    return true;
}

bool print_diagnostics(const forge::Diagnostics& diagnostics, std::string_view file_name = {}, std::string_view source = {}) {
    std::cerr << forge::diagnostics::render_all(diagnostics, {file_name, source});
    for (const auto& diagnostic : diagnostics)
        if (diagnostic.severity == forge::DiagnosticSeverity::error) return true;
    return false;
}

std::optional<forge::ir::Module> load_verified(const std::string& path) {
    std::string source;
    if (!read_file(path, source)) return std::nullopt;
    auto parsed = forge::ir::parse_module(source);
    if (!parsed.ok()) {
        print_diagnostics(parsed.diagnostics, path, source);
        return std::nullopt;
    }
    const auto diagnostics = forge::ir::verify_module(*parsed.module);
    if (print_diagnostics(diagnostics)) return std::nullopt;
    return std::move(*parsed.module);
}
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string_view command = argv[1];
    if (command == "version" || command == "--version" || command == "-V") {
        std::cout << "forge " << FORGE_VERSION_STRING << '\n';
        return 0;
    }
    if (command == "--help" || command == "-h" || command == "help") {
        usage();
        return 0;
    }
    if ((command == "verify" || command == "print") && argc == 3) {
        auto module = load_verified(argv[2]);
        if (!module) return 1;
        if (command == "print") std::cout << forge::ir::print_module(*module);
        else std::cout << "FORGE  verified " << argv[2] << '\n';
        return 0;
    }
    if (command == "compile" && argc >= 5) {
        std::string_view format = "auto";
        std::string output_path;
        auto level = forge::pass::OptimizationLevel::o2;
        bool pass_stats = false;
        for (int index = 3; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument == "-o" && index + 1 < argc) output_path = argv[++index];
            else if (argument.rfind("--format=", 0) == 0) format = argument.substr(9);
            else if (argument == "--pass-stats") pass_stats = true;
            else if (const auto parsed_level = forge::pass::parse_optimization_level(argument)) level = *parsed_level;
            else { std::cerr << "error: unknown compile option " << argument << '\n'; return 2; }
        }
        if (output_path.empty()) { std::cerr << "error: compile requires -o <file>\n"; return 2; }
        if (format == "auto") {
#if defined(_WIN32)
            format = "coff";
#else
            format = "elf";
#endif
        }
        auto module = load_verified(argv[2]);
        if (!module) return 1;
        forge::pass::PassManager pipeline;
        forge::pass::build_standard_pipeline(pipeline, level);
        try {
            const auto report = pipeline.run_with_report(*module);
            if (pass_stats) {
                std::cerr << "FORGE  pipeline -" << forge::pass::optimization_level_name(level)
                          << " passes=" << pipeline.pass_names().size()
                          << " rewritten=" << report.total.operations_rewritten
                          << " removed=" << report.total.operations_removed
                          << " blocks=" << report.total.blocks_removed << '\n';
            }
        } catch (const std::exception& error) {
            std::cerr << "error: " << error.what() << '\n';
            return 1;
        }
        auto lowered = forge::machine::lower_module(*module);
        if (!lowered.ok()) { print_diagnostics(lowered.diagnostics); return 1; }
        std::vector<std::byte> bytes;
        std::string_view label;
        if (format == "elf") {
            auto object = forge::object::emit_elf64_x86_64(*lowered.module, forge::codegen::x86_64::Abi::system_v);
            if (!object.ok()) { print_diagnostics(object.diagnostics); return 1; }
            bytes = std::move(object.bytes); label = "ELF64";
        } else if (format == "coff") {
            auto object = forge::object::emit_coff_x86_64(*lowered.module, forge::codegen::x86_64::Abi::windows);
            if (!object.ok()) { print_diagnostics(object.diagnostics); return 1; }
            bytes = std::move(object.bytes); label = "COFF AMD64";
        } else { std::cerr << "error: expected --format=auto, elf, or coff\n"; return 2; }
        std::ofstream output(output_path, std::ios::binary);
        if (!output) { std::cerr << "error: cannot create " << output_path << '\n'; return 1; }
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) { std::cerr << "error: cannot write " << output_path << '\n'; return 1; }
        std::cout << "FORGE  emitted " << label << " object " << output_path << " (" << bytes.size() << " bytes)\n";
        return 0;
    }
    if (command == "opt" && (argc == 3 || argc == 4)) {
        auto level = forge::pass::OptimizationLevel::o2;
        if (argc == 4) {
            const auto parsed_level = forge::pass::parse_optimization_level(argv[3]);
            if (!parsed_level) {
                std::cerr << "error: expected -O0, -O1, -O2, -O3, -Os, or -Oz\n";
                return 2;
            }
            level = *parsed_level;
        }
        auto module = load_verified(argv[2]);
        if (!module) return 1;
        forge::pass::PassManager pipeline;
        forge::pass::build_standard_pipeline(pipeline, level);
        try {
            const auto result = pipeline.run(*module);
            (void)result;
        } catch (const std::exception& error) {
            std::cerr << "error: " << error.what() << '\n';
            return 1;
        }
        std::cout << forge::ir::print_module(*module);
        return 0;
    }
    usage();
    return 2;
}
