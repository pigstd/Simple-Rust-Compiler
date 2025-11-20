#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

fs::path find_fixtures_dir() {
    auto current = fs::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        auto candidate = current / "test" / "IRBuilder" / "fixtures";
        if (fs::exists(candidate)) {
            return candidate;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}
} // namespace

int main() {
    const auto fixtures_dir = find_fixtures_dir();
    if (fixtures_dir.empty()) {
        std::cerr << "Failed to locate test/IRBuilder/fixtures directory\n";
        return 1;
    }

    const std::vector<std::pair<std::string, std::vector<std::string>>> expectations = {
        {"simple_return.ir.expected", {"@main", "ret i32 0"}},
        {"print_literal.ir.expected",
         {"%StrLiteral = type", "declare void @print(%StrLiteral)",
          "alloca %StrLiteral", "call void @print(%StrLiteral"}},
        {"local_alloca_store.ir.expected", {"alloca i32", "store i32 5", "load i32"}},
        {"branch_compare.ir.expected", {"icmp sgt", "br i1", "label %then.0", "label %else.0"}},
        {"loop_and_call.ir.expected",
         {"%LoopMsg = type", "declare void @print(%LoopMsg)",
          "alloca %LoopMsg", "call void @print(%LoopMsg"}},
        {"array_access.ir.expected", {"alloca [3 x i32]", "getelementptr [3 x i32]", "ret i32 %sum"}},
        {"struct_usage.ir.expected", {"%Point = type", "getelementptr %Point", "ret i32 %sum"}},
        {"loop_with_continue.ir.expected", {"loop.continue", "br label %loop.cond", "and i32", "ret i32 %result"}},
        {"bitwise_ops.ir.expected", {"and i32", "or i32", "xor i32", "sdiv i32", "udiv i32", "srem i32", "urem i32"}},
        {"function_multiple_returns.ir.expected", {"ret i32 %min", "ret i32 %max", "ret i32 %value"}},
        {"loop_with_function_call.ir.expected", {"call i32 @helper", "br label %loop.cond", "ret i32 %result"}},
        {"call_auto_name.ir.expected",
         {"%0 = call i32 @helper", "%1 = call i32 @helper", "ret i32 %1"}},
        {"runtime_builtins.ir.expected",
         {"%Str = type", "%String = type", "declare void @print",
          "declare %String @String_from", "declare i32 @Array_len"}},
    };

    for (const auto &fixture : expectations) {
        const auto path = fixtures_dir / fixture.first;
        std::ifstream input(path);
        if (!input.is_open()) {
            std::cerr << "Unable to open fixture: " << path << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << input.rdbuf();
        const auto text = buffer.str();
        if (text.empty()) {
            std::cerr << "Fixture was empty: " << path << "\n";
            return 1;
        }
        for (const auto &token : fixture.second) {
            if (text.find(token) == std::string::npos) {
                std::cerr << "Token '" << token << "' missing in fixture: " << path << "\n";
                return 1;
            }
        }
    }

    return 0;
}
