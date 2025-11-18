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
        auto candidate = current / "test" / "ir" / "fixtures";
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
        std::cerr << "Failed to locate test/ir/fixtures directory\n";
        return 1;
    }

    const std::vector<std::pair<std::string, std::vector<std::string>>> expectations = {
        {"simple_return.ir.expected", {"@main", "ret i32 0"}},
        {"print_literal.ir.expected", {"@print", "@.str.0", "call void @print"}},
        {"local_alloca_store.ir.expected", {"alloca i32", "store i32 5", "load i32"}},
        {"branch_compare.ir.expected", {"icmp sgt", "br i1", "label %then.0", "label %else.0"}},
        {"loop_and_call.ir.expected", {"br label %loop.cond", "call void @print", "@.str.loop"}},
        {"array_access.ir.expected", {"alloca [3 x i32]", "getelementptr [3 x i32]", "ret i32 %sum"}},
        {"struct_usage.ir.expected", {"%Point = type", "getelementptr %Point", "ret i32 %sum"}},
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
