#include "ir/type_lowering.h"
#include <cassert>
#include <string>

using namespace ir;

int main() {
    IRModule module("unknown-unknown-unknown", "");
    TypeLowering lowering(module);
    lowering.declare_builtin_string_types();
    const std::string expected =
        "target triple = \"unknown-unknown-unknown\"\n"
        "target datalayout = \"\"\n"
        "\n"
        "%Str = type { ptr, i32 }\n"
        "%String = type { ptr, i32, i32 }\n"
        "\n";
    assert(module.to_string() == expected);
    return 0;
}
