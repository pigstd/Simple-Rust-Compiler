#include "ir/IRBuilder.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[maybe_unused]] fs::path find_fixtures_dir() {
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

[[maybe_unused]] std::string read_file_to_string(const fs::path &path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open fixture: " + path.string());
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

[[maybe_unused]] std::string normalize(std::string text) {
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    if (!text.empty() && text.back() != '\n') {
        text.push_back('\n');
    }
    return text;
}

} // namespace

using FixtureBuilderFn = std::function<std::string()>;

struct BuilderEnv {
    ir::IRModule module;
    ir::IRBuilder builder;
    ir::IRType_ptr void_type;
    ir::IRType_ptr i1_type;
    ir::IRType_ptr i8_type;
    ir::IRType_ptr i32_type;

    BuilderEnv() : module("unknown-unknown-unknown", ""), builder(module) {
        void_type = std::make_shared<ir::VoidType>();
        i1_type = std::make_shared<ir::IntegerType>(1);
        i8_type = std::make_shared<ir::IntegerType>(8);
        i32_type = std::make_shared<ir::IntegerType>(32);
    }

    ir::ConstantValue_ptr iconst(int64_t value) const {
        return std::make_shared<ir::ConstantValue>(i32_type, value);
    }

    ir::ConstantValue_ptr bool_const(bool value) const {
        return std::make_shared<ir::ConstantValue>(i1_type, value ? 1 : 0);
    }

    ir::StructType_ptr register_struct(const std::string &name,
                                       std::vector<ir::IRType_ptr> fields) {
        std::vector<std::string> field_texts;
        field_texts.reserve(fields.size());
        for (const auto &field : fields) {
            field_texts.push_back(field->to_string());
        }
        module.add_type_definition(name, field_texts);
        auto type = std::make_shared<ir::StructType>(name);
        type->set_fields(std::move(fields));
        return type;
    }
};

std::string build_simple_return() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: simple function returning constant 0");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    auto fn = env.module.define_function("main", fn_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    env.builder.create_ret(env.iconst(0));
    return env.module.to_string();
}

std::string build_print_literal() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: print builtin called with &str literal");
    auto ptr_type = std::make_shared<ir::PointerType>();
    auto str_struct =
        env.register_struct("StrLiteral", {ptr_type, env.i32_type});
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    env.module.declare_function(
        "print",
        std::make_shared<ir::FunctionType>(
            env.void_type, std::vector<ir::IRType_ptr>{str_struct}),
        false);
    auto str_array_type = std::make_shared<ir::ArrayType>(env.i8_type, 3);
    auto str_global = env.module.create_global(".str.0", str_array_type,
                                               "c\"hi\\00\"", true, "private");
    auto fn = env.module.define_function("main", fn_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    auto tmp = env.builder.create_alloca(str_struct, "tmp.0");
    auto ptr_field = env.builder.create_gep(
        tmp, str_struct, {env.iconst(0), env.iconst(0)}, "ptr.field");
    auto len_field = env.builder.create_gep(
        tmp, str_struct, {env.iconst(0), env.iconst(1)}, "len.field");
    auto msg_ptr = env.builder.create_gep(
        str_global, str_array_type, {env.iconst(0), env.iconst(0)}, "msg.ptr");
    env.builder.create_store(msg_ptr, ptr_field);
    env.builder.create_store(env.iconst(2), len_field);
    auto packed = env.builder.create_load(tmp, "tmp.1");
    env.builder.create_call("print", {packed}, env.void_type, "");
    env.builder.create_ret(env.iconst(0));
    return env.module.to_string();
}

std::string build_local_alloca_store() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: stack allocation, store, load and return");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    auto fn = env.module.define_function("main", fn_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    auto slot = env.builder.create_alloca(env.i32_type, "tmp.0");
    env.builder.create_store(env.iconst(5), slot);
    auto loaded = env.builder.create_load(slot, "tmp.1");
    env.builder.create_ret(loaded);
    return env.module.to_string();
}

std::string build_branch_compare() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: comparison driving conditional branch");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{env.i32_type, env.i32_type});
    auto fn = env.module.define_function("max", fn_type);
    auto a = fn->add_param("a", env.i32_type);
    auto b = fn->add_param("b", env.i32_type);
    auto entry = fn->create_block("entry");
    auto then_block = fn->create_block("then");
    auto else_block = fn->create_block("else");
    env.builder.set_insertion_point(entry);
    auto cmp = env.builder.create_icmp_sgt(a, b, "cmp.0");
    env.builder.create_cond_br(cmp, then_block, else_block);
    env.builder.set_insertion_point(then_block);
    env.builder.create_ret(a);
    env.builder.set_insertion_point(else_block);
    env.builder.create_ret(b);
    return env.module.to_string();
}

std::string build_array_access() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: stack array, element store and load");
    auto arr_type = std::make_shared<ir::ArrayType>(env.i32_type, 3);
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    auto fn = env.module.define_function("sum_array", fn_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    auto arr = env.builder.create_alloca(arr_type, "arr");
    auto elem0 = env.builder.create_gep(
        arr, arr_type, {env.iconst(0), env.iconst(0)}, "elem0");
    auto elem1 = env.builder.create_gep(
        arr, arr_type, {env.iconst(0), env.iconst(1)}, "elem1");
    auto elem2 = env.builder.create_gep(
        arr, arr_type, {env.iconst(0), env.iconst(2)}, "elem2");
    env.builder.create_store(env.iconst(1), elem0);
    env.builder.create_store(env.iconst(2), elem1);
    env.builder.create_store(env.iconst(3), elem2);
    auto v0 = env.builder.create_load(elem0, "v0");
    auto v1 = env.builder.create_load(elem1, "v1");
    auto v2 = env.builder.create_load(elem2, "v2");
    auto tmp = env.builder.create_add(v0, v1, "tmp");
    auto sum = env.builder.create_add(tmp, v2, "sum");
    env.builder.create_ret(sum);
    return env.module.to_string();
}

std::string build_loop_and_call() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: loop with counter and runtime call");
    auto ptr_type = std::make_shared<ir::PointerType>();
    auto str_struct = env.register_struct("LoopMsg", {ptr_type, env.i32_type});
    env.module.declare_function(
        "print",
        std::make_shared<ir::FunctionType>(
            env.void_type, std::vector<ir::IRType_ptr>{str_struct}),
        false);
    auto str_array_type = std::make_shared<ir::ArrayType>(env.i8_type, 4);
    auto str_global = env.module.create_global(".str.loop", str_array_type,
                                               "c\"cnt\\00\"", true, "private");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    auto fn = env.module.define_function("main", fn_type);
    auto entry = fn->create_block("entry");
    auto loop_cond = fn->create_block("loop.cond");
    auto loop_body = fn->create_block("loop.body");
    auto loop_end = fn->create_block("loop.end");
    env.builder.set_insertion_point(entry);
    auto i_addr = env.builder.create_alloca(env.i32_type, "i.addr");
    auto msg_alloca = env.builder.create_alloca(str_struct, "msg.alloca");
    auto ptr_field = env.builder.create_gep(
        msg_alloca, str_struct, {env.iconst(0), env.iconst(0)}, "ptr.field");
    auto len_field = env.builder.create_gep(
        msg_alloca, str_struct, {env.iconst(0), env.iconst(1)}, "len.field");
    auto msg_ptr = env.builder.create_gep(
        str_global, str_array_type, {env.iconst(0), env.iconst(0)}, "msg.ptr");
    env.builder.create_store(msg_ptr, ptr_field);
    env.builder.create_store(env.iconst(3), len_field);
    env.builder.create_store(env.iconst(0), i_addr);
    env.builder.create_br(loop_cond);

    env.builder.set_insertion_point(loop_cond);
    auto i_val = env.builder.create_load(i_addr, "i.val");
    auto cmp = env.builder.create_icmp_slt(i_val, env.iconst(5), "cmp");
    env.builder.create_cond_br(cmp, loop_body, loop_end);

    env.builder.set_insertion_point(loop_body);
    auto msg_value = env.builder.create_load(msg_alloca, "msg.value");
    env.builder.create_call("print", {msg_value}, env.void_type, "");
    auto next = env.builder.create_add(i_val, env.iconst(1), "next");
    env.builder.create_store(next, i_addr);
    env.builder.create_br(loop_cond);

    env.builder.set_insertion_point(loop_end);
    env.builder.create_ret(env.iconst(0));
    return env.module.to_string();
}

std::string build_struct_usage() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: struct stack allocation and field manipulation");
    auto point_type =
        env.register_struct("Point", {env.i32_type, env.i32_type});
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    auto fn = env.module.define_function("use_point", fn_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    auto p = env.builder.create_alloca(point_type, "p");
    auto x_ptr = env.builder.create_gep(
        p, point_type, {env.iconst(0), env.iconst(0)}, "x.ptr");
    auto y_ptr = env.builder.create_gep(
        p, point_type, {env.iconst(0), env.iconst(1)}, "y.ptr");
    env.builder.create_store(env.iconst(10), x_ptr);
    env.builder.create_store(env.iconst(20), y_ptr);
    auto x = env.builder.create_load(x_ptr, "x");
    auto y = env.builder.create_load(y_ptr, "y");
    auto sum = env.builder.create_add(x, y, "sum");
    env.builder.create_ret(sum);
    return env.module.to_string();
}

std::string build_function_multiple_returns() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: function with multiple return blocks and shared exit");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type,
        std::vector<ir::IRType_ptr>{env.i32_type, env.i32_type, env.i32_type});
    auto fn = env.module.define_function("clamp", fn_type);
    auto value = fn->add_param("value", env.i32_type);
    auto min_param = fn->add_param("min", env.i32_type);
    auto max_param = fn->add_param("max", env.i32_type);
    auto entry = fn->create_block("entry");
    auto ret_min = fn->create_block("ret.min");
    auto check_max = fn->create_block("check.max");
    auto ret_max = fn->create_block("ret.max");
    auto ret_value = fn->create_block("ret.value");

    env.builder.set_insertion_point(entry);
    auto lt_min = env.builder.create_icmp_slt(value, min_param, "lt_min");
    env.builder.create_cond_br(lt_min, ret_min, check_max);
    env.builder.set_insertion_point(ret_min);
    env.builder.create_ret(min_param);

    env.builder.set_insertion_point(check_max);
    auto gt_max = env.builder.create_icmp_sgt(value, max_param, "gt_max");
    env.builder.create_cond_br(gt_max, ret_max, ret_value);
    env.builder.set_insertion_point(ret_max);
    env.builder.create_ret(max_param);
    env.builder.set_insertion_point(ret_value);
    env.builder.create_ret(value);
    return env.module.to_string();
}

std::string build_bitwise_ops() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: coverage for bitwise, shift, and division instructions");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{env.i32_type, env.i32_type});
    auto fn = env.module.define_function("bitwise_ops", fn_type);
    auto a = fn->add_param("a", env.i32_type);
    auto b = fn->add_param("b", env.i32_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    auto v_and = env.builder.create_and(a, b, "and");
    auto v_or = env.builder.create_or(a, b, "or");
    auto v_xor = env.builder.create_xor(a, b, "xor");
    auto v_shl = env.builder.create_shl(a, env.iconst(2), "shl");
    auto v_lshr = env.builder.create_lshr(a, env.iconst(1), "lshr");
    auto v_ashr = env.builder.create_ashr(a, env.iconst(1), "ashr");
    auto v_sdiv = env.builder.create_sdiv(a, b, "sdiv");
    auto v_udiv = env.builder.create_udiv(a, b, "udiv");
    auto v_srem = env.builder.create_srem(a, b, "srem");
    auto v_urem = env.builder.create_urem(a, b, "urem");
    auto tmp0 = env.builder.create_add(v_and, v_or, "tmp0");
    auto tmp1 = env.builder.create_add(tmp0, v_xor, "tmp1");
    auto tmp2 = env.builder.create_add(tmp1, v_shl, "tmp2");
    auto tmp3 = env.builder.create_add(tmp2, v_lshr, "tmp3");
    auto tmp4 = env.builder.create_add(tmp3, v_ashr, "tmp4");
    auto tmp5 = env.builder.create_add(tmp4, v_sdiv, "tmp5");
    auto tmp6 = env.builder.create_add(tmp5, v_udiv, "tmp6");
    auto tmp7 = env.builder.create_add(tmp6, v_srem, "tmp7");
    auto sum = env.builder.create_add(tmp7, v_urem, "sum");
    env.builder.create_ret(sum);
    return env.module.to_string();
}

std::string build_loop_with_continue() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: explicit continue block with multiple loop labels");
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{env.i32_type});
    auto fn = env.module.define_function("sum_even_under", fn_type);
    auto limit = fn->add_param("limit", env.i32_type);

    auto entry = fn->create_block("entry");
    auto loop_cond = fn->create_block("loop.cond");
    auto loop_body = fn->create_block("loop.body");
    auto loop_acc = fn->create_block("loop.accumulate");
    auto loop_continue = fn->create_block("loop.continue");
    auto loop_exit = fn->create_block("loop.exit");

    env.builder.set_insertion_point(entry);
    auto i_addr = env.builder.create_alloca(env.i32_type, "i.addr");
    auto sum_addr = env.builder.create_alloca(env.i32_type, "sum.addr");
    env.builder.create_store(env.iconst(0), i_addr);
    env.builder.create_store(env.iconst(0), sum_addr);
    env.builder.create_br(loop_cond);

    env.builder.set_insertion_point(loop_cond);
    auto i_val = env.builder.create_load(i_addr, "i.val");
    auto has_next = env.builder.create_icmp_slt(i_val, limit, "has_next");
    env.builder.create_cond_br(has_next, loop_body, loop_exit);

    env.builder.set_insertion_point(loop_body);
    auto mask = env.builder.create_and(i_val, env.iconst(1), "is_even_mask");
    auto is_even = env.builder.create_icmp_eq(mask, env.iconst(0), "is_even");
    env.builder.create_cond_br(is_even, loop_acc, loop_continue);

    env.builder.set_insertion_point(loop_acc);
    auto current = env.builder.create_load(sum_addr, "current");
    auto updated = env.builder.create_add(current, i_val, "updated");
    env.builder.create_store(updated, sum_addr);
    env.builder.create_br(loop_continue);

    env.builder.set_insertion_point(loop_continue);
    auto next = env.builder.create_add(i_val, env.iconst(1), "next");
    env.builder.create_store(next, i_addr);
    env.builder.create_br(loop_cond);

    env.builder.set_insertion_point(loop_exit);
    auto result = env.builder.create_load(sum_addr, "result");
    env.builder.create_ret(result);
    return env.module.to_string();
}

std::string build_loop_with_function_call() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: loop nesting with inner function call and accumulator return");
    auto helper_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{env.i32_type});
    env.module.declare_function("helper", helper_type, false);
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{env.i32_type});
    auto fn = env.module.define_function("loop_with_call", fn_type);
    auto limit = fn->add_param("limit", env.i32_type);

    auto entry = fn->create_block("entry");
    auto loop_cond = fn->create_block("loop.cond");
    auto loop_body = fn->create_block("loop.body");
    auto loop_exit = fn->create_block("loop.exit");

    env.builder.set_insertion_point(entry);
    auto i_addr = env.builder.create_alloca(env.i32_type, "i.addr");
    auto acc_addr = env.builder.create_alloca(env.i32_type, "acc.addr");
    env.builder.create_store(env.iconst(0), i_addr);
    env.builder.create_store(env.iconst(0), acc_addr);
    env.builder.create_br(loop_cond);

    env.builder.set_insertion_point(loop_cond);
    auto i_val = env.builder.create_load(i_addr, "i.val");
    auto has_next = env.builder.create_icmp_slt(i_val, limit, "has_next");
    env.builder.create_cond_br(has_next, loop_body, loop_exit);

    env.builder.set_insertion_point(loop_body);
    auto call =
        env.builder.create_call("helper", {i_val}, env.i32_type, "call");
    auto acc = env.builder.create_load(acc_addr, "acc");
    auto new_acc = env.builder.create_add(acc, call, "new.acc");
    env.builder.create_store(new_acc, acc_addr);
    auto inc = env.builder.create_add(i_val, env.iconst(1), "inc");
    env.builder.create_store(inc, i_addr);
    env.builder.create_br(loop_cond);

    env.builder.set_insertion_point(loop_exit);
    auto result = env.builder.create_load(acc_addr, "result");
    env.builder.create_ret(result);
    return env.module.to_string();
}

std::string build_call_auto_name() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: call without name hint uses auto numbered temps");
    auto helper_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{env.i32_type});
    env.module.declare_function("helper", helper_type, false);
    auto fn_type = std::make_shared<ir::FunctionType>(
        env.i32_type, std::vector<ir::IRType_ptr>{});
    auto fn = env.module.define_function("use_calls", fn_type);
    auto entry = fn->create_block("entry");
    env.builder.set_insertion_point(entry);
    auto first = env.builder.create_call("helper", {env.iconst(1)}, env.i32_type);
    auto second = env.builder.create_call("helper", {first}, env.i32_type);
    env.builder.create_ret(second);
    return env.module.to_string();
}

std::string build_runtime_builtins() {
    BuilderEnv env;
    env.module.add_module_comment(
        "; EXPECT: ensure_runtime_builtins injects builtin declarations");
    auto ptr_type = std::make_shared<ir::PointerType>();
    env.register_struct("Str", {ptr_type, env.i32_type});
    env.register_struct("String",
                        {ptr_type, env.i32_type, env.i32_type});
    env.module.ensure_runtime_builtins();
    return env.module.to_string();
}

int main() {
    const auto fixtures_dir = find_fixtures_dir();
    if (fixtures_dir.empty()) {
        std::cerr << "Failed to locate test/IRBuilder/fixtures directory\n";
        return 1;
    }

    const std::vector<std::pair<std::string, FixtureBuilderFn>> builders = {
        {"simple_return.ir.expected", build_simple_return},
        {"print_literal.ir.expected", build_print_literal},
        {"local_alloca_store.ir.expected", build_local_alloca_store},
        {"branch_compare.ir.expected", build_branch_compare},
        {"loop_and_call.ir.expected", build_loop_and_call},
        {"array_access.ir.expected", build_array_access},
        {"struct_usage.ir.expected", build_struct_usage},
        {"function_multiple_returns.ir.expected",
         build_function_multiple_returns},
        {"bitwise_ops.ir.expected", build_bitwise_ops},
        {"loop_with_continue.ir.expected", build_loop_with_continue},
        {"loop_with_function_call.ir.expected", build_loop_with_function_call},
        {"call_auto_name.ir.expected", build_call_auto_name},
        {"runtime_builtins.ir.expected", build_runtime_builtins},
    };

    bool failed = false;
    for (const auto &entry : builders) {
        const auto path = fixtures_dir / entry.first;
        const auto expected = normalize(read_file_to_string(path));
        const auto actual = normalize(entry.second());
        if (expected != actual) {
            std::cerr << "Fixture mismatch for " << entry.first << "\n";
            std::cerr << "----- expected -----\n"
                      << expected << "----- actual -----\n"
                      << actual;
            failed = true;
        }
    }

    if (failed) {
        return 1;
    }
    std::cout << "IR builder fixtures matched all expectations (" << builders.size()
              << " cases)." << std::endl;
    return 0;
}
