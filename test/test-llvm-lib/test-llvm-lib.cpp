// 测试 LLVM IR 生成能否编译
// 可以运行就说明我现在已经有 llvm IR 库了

// generate by AI
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

int main() {
    // 1. 创建 LLVM 上下文（管理类型、常量池等全局状态）
    LLVMContext Context;

    // 2. 创建一个模块（相当于一个 .ll 文件或编译单元）
    std::unique_ptr<Module> TheModule = std::make_unique<Module>("my_module", Context);

    // 3. 准备函数签名：int(int, int)
    Type *Int32Ty = Type::getInt32Ty(Context);
    std::vector<Type *> Args(2, Int32Ty);
    FunctionType *FuncTy = FunctionType::get(Int32Ty, Args, false);

    // 4. 创建函数对象
    Function *AddFunc = Function::Create(FuncTy, Function::ExternalLinkage, "add", TheModule.get());

    // 为参数命名
    auto ArgIter = AddFunc->arg_begin();
    ArgIter->setName("a");
    (++ArgIter)->setName("b");

    // 5. 创建基本块和 IRBuilder
    BasicBlock *EntryBB = BasicBlock::Create(Context, "entry", AddFunc);
    IRBuilder<> Builder(EntryBB);

    // 6. 生成加法指令并返回
    Value *A = AddFunc->getArg(0);
    Value *B = AddFunc->getArg(1);
    Value *Sum = Builder.CreateAdd(A, B, "sum");
    Builder.CreateRet(Sum);

    // 7. 验证 IR 是否正确
    verifyFunction(*AddFunc);

    // 8. 打印生成的 IR
    TheModule->print(outs(), nullptr);

    return 0;
}