#include "ir/IRGen.h"
#include <cassert>

using namespace ir;

int main() {
    FunctionContext ctx;
    assert(ctx.decl == nullptr);
    assert(ctx.ir_function == nullptr);
    assert(ctx.entry_block == nullptr);
    assert(ctx.current_block == nullptr);
    assert(ctx.return_block == nullptr);
    assert(ctx.return_slot == nullptr);
    assert(ctx.self_slot == nullptr);
    assert(!ctx.block_sealed);
    assert(ctx.local_slots.empty());
    assert(ctx.loop_stack.empty());

    LoopContext loop;
    assert(loop.header_block == nullptr);
    assert(loop.body_block == nullptr);
    assert(loop.continue_target == nullptr);
    assert(loop.break_target == nullptr);
    assert(loop.break_slot == nullptr);
    assert(loop.result_type == nullptr);
    return 0;
}
