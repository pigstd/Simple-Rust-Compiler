实现 PLAN.md 中的第三步：**全局项生成**  

文档 `docs/IR/global-lowering.md` 已经给出所有约束，现在需要按文档实现：
- 基于 `Semantic_Checker` 的 `root_scope` 与 `const_value_map`，实现 `GlobalLoweringDriver`，在一次 DFS 中依次处理结构体、函数、数组常量，并把结果注册到 `IRModule`。
- 遵循文档描述的符号命名方案（`scope_suffix_stack_` + `allocate_symbol`），在 `emit_function_decl`/`emit_const` 中回写唯一化后的名字，防止 `len` 之类的重名。
- 数组常量必须直接使用 `const_value_map` 给出的值展开，不允许复用其他常量的指针；其它类型的 `const` 暂不生成 IR。
- 为该阶段补充测试：构造若干真实源码，跑到语义阶段后调用 `GlobalLoweringDriver`，断言结构体定义、函数声明、数组全局与命名规则都符合 `docs/IR/global-lowering.md`。
