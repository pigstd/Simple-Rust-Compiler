#include "ir/IRBuilder.h"

#include <string>
#include <type_traits>
#include <vector>

namespace {
using BinaryOpSignature = ir::IRValue_ptr (ir::IRBuilder::*)(
    ir::IRValue_ptr, ir::IRValue_ptr, const std::string &);
using StoreSignature = void (ir::IRBuilder::*)(ir::IRValue_ptr,
                                               ir::IRValue_ptr);
using BranchSignature = void (ir::IRBuilder::*)(ir::BasicBlock_ptr);
using CondBranchSignature = void (ir::IRBuilder::*)(ir::IRValue_ptr,
                                                    ir::BasicBlock_ptr,
                                                    ir::BasicBlock_ptr);
using RetSignature = void (ir::IRBuilder::*)(ir::IRValue_ptr);
using RetVoidSignature = void (ir::IRBuilder::*)();
using CallSignature = ir::IRValue_ptr (ir::IRBuilder::*)(
    const std::string &, const std::vector<ir::IRValue_ptr> &, ir::IRType_ptr,
    const std::string &);
using LoadSignature = ir::IRValue_ptr (ir::IRBuilder::*)(ir::IRValue_ptr,
                                                         const std::string &);
using AllocaSignature = ir::IRValue_ptr (ir::IRBuilder::*)(ir::IRType_ptr,
                                                           const std::string &);
using TempAllocaSignature =
    ir::IRValue_ptr (ir::IRBuilder::*)(ir::IRType_ptr, const std::string &);
using TempSignature = ir::IRValue_ptr (ir::IRBuilder::*)(ir::IRType_ptr,
                                                         const std::string &);
using GEPAssignment = ir::IRValue_ptr (ir::IRBuilder::*)(
    ir::IRValue_ptr, ir::IRType_ptr, const std::vector<ir::IRValue_ptr> &,
    const std::string &);
using UnaryOpSignature =
    ir::IRValue_ptr (ir::IRBuilder::*)(ir::IRValue_ptr, const std::string &);
} // namespace

static_assert(std::is_abstract_v<ir::IRType>);
static_assert(std::is_base_of_v<ir::IRType, ir::IntegerType>);
static_assert(std::is_base_of_v<ir::IRType, ir::StructType>);
static_assert(std::is_base_of_v<ir::IRValue, ir::RegisterValue>);
static_assert(std::is_base_of_v<ir::IRValue, ir::GlobalValue>);
static_assert(std::is_same_v<ir::ConstantLiteral, int64_t>);

static_assert(std::is_constructible_v<ir::VoidType>);
static_assert(std::is_constructible_v<ir::IntegerType, int>);
static_assert(std::is_constructible_v<ir::PointerType>);
static_assert(
    std::is_constructible_v<ir::ArrayType, ir::IRType_ptr, std::size_t>);
static_assert(std::is_constructible_v<ir::StructType, std::string>);
static_assert(std::is_constructible_v<ir::FunctionType, ir::IRType_ptr,
                                      std::vector<ir::IRType_ptr>>);
static_assert(std::is_constructible_v<ir::ConstantValue, ir::IRType_ptr,
                                      ir::ConstantLiteral>);
static_assert(
    std::is_constructible_v<ir::RegisterValue, std::string, ir::IRType_ptr>);

static_assert(
    std::is_constructible_v<ir::IRInstruction, ir::Opcode,
                            std::vector<ir::IRValue_ptr>, ir::IRValue_ptr>);
static_assert(std::is_constructible_v<ir::BasicBlock, std::string>);
static_assert(
    std::is_constructible_v<ir::IRFunction, std::string, ir::IRType_ptr, bool>);
static_assert(std::is_constructible_v<ir::IRModule, std::string, std::string>);
static_assert(std::is_constructible_v<ir::IRSerializer, const ir::IRModule &>);
static_assert(std::is_constructible_v<ir::IRBuilder, ir::IRModule &>);

static_assert(std::is_same_v<ir::IRValue_ptr (ir::IRFunction::*)(
                                 const std::string &, ir::IRType_ptr),
                             decltype(&ir::IRFunction::add_param)>);
static_assert(
    std::is_same_v<ir::BasicBlock_ptr (ir::IRFunction::*)(const std::string &),
                   decltype(&ir::IRFunction::create_block)>);

static_assert(
    std::is_same_v<ir::GlobalValue_ptr (ir::IRModule::*)(
                       const std::string &, ir::IRType_ptr, const std::string &,
                       bool, const std::string &),
                   decltype(&ir::IRModule::create_global)>);
static_assert(std::is_same_v<ir::IRFunction_ptr (ir::IRModule::*)(
                                 const std::string &, ir::IRType_ptr, bool),
                             decltype(&ir::IRModule::declare_function)>);
static_assert(std::is_same_v<ir::IRFunction_ptr (ir::IRModule::*)(
                                 const std::string &, ir::IRType_ptr),
                             decltype(&ir::IRModule::define_function)>);

static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_add)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_sub)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_mul)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_sdiv)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_udiv)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_srem)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_urem)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_and)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_or)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_xor)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_shl)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_lshr)>);
static_assert(
    std::is_same_v<BinaryOpSignature, decltype(&ir::IRBuilder::create_ashr)>);

static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_eq)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_ne)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_slt)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_sle)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_sgt)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_sge)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_ult)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_ule)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_ugt)>);
static_assert(std::is_same_v<BinaryOpSignature,
                             decltype(&ir::IRBuilder::create_icmp_uge)>);

static_assert(
    std::is_same_v<UnaryOpSignature, decltype(&ir::IRBuilder::create_not)>);

static_assert(
    std::is_same_v<AllocaSignature, decltype(&ir::IRBuilder::create_alloca)>);
static_assert(
    std::is_same_v<LoadSignature, decltype(&ir::IRBuilder::create_load)>);
static_assert(
    std::is_same_v<StoreSignature, decltype(&ir::IRBuilder::create_store)>);
static_assert(
    std::is_same_v<GEPAssignment, decltype(&ir::IRBuilder::create_gep)>);
static_assert(std::is_same_v<TempAllocaSignature,
                             decltype(&ir::IRBuilder::create_temp_alloca)>);
static_assert(
    std::is_same_v<TempSignature, decltype(&ir::IRBuilder::create_temp)>);

static_assert(
    std::is_same_v<BranchSignature, decltype(&ir::IRBuilder::create_br)>);
static_assert(std::is_same_v<CondBranchSignature,
                             decltype(&ir::IRBuilder::create_cond_br)>);
static_assert(
    std::is_same_v<RetSignature, decltype(&ir::IRBuilder::create_ret)>);

static_assert(
    std::is_same_v<CallSignature, decltype(&ir::IRBuilder::create_call)>);
static_assert(
    std::is_same_v<ir::GlobalValue_ptr (ir::IRBuilder::*)(const std::string &),
                   decltype(&ir::IRBuilder::create_string_literal)>);

int main() { return 0; }
