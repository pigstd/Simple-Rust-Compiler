### IR/Statements & Expr Lowering 模块

本章节覆盖 PLAN.md 第四阶段：为所有函数体、语句与表达式生成可执行的 IR。前两步（IRBuilder、TypeLowering）提供了类型/指令基石，第三步（global lowering）已经把结构体、常量与函数声明注册到 `IRModule`。本阶段在这些成果上继续向前，准备好每个 `FnDecl` 的基本块、局部栈槽，并把 AST 节点逐一翻译成内存形式的 IR（以 `alloca + store/load` 为核心，不引入 `phi` 指令）。目标是覆盖语言目前支持的全部语法：`let`、赋值、算术/逻辑、结构体与数组访问、循环控制流、方法与关联函数调用等。

#### 设计目标
- 依据语义阶段产物（`node_scope_map`、`node_type_and_place_kind_map`、`scope_local_variable_map`、`node_outcome_state_map`、`call_expr_to_decl_map`、`const_value_map`）精确决定表达式的真实类型、左值属性和控制流终结状态，生成无冗余的基本块、`load/store`、`br` 指令。
- 为每个 `FnItem` 构建一致的“内存形式”函数体：所有表达式值要么写入调用方提供的 `alloca` 槽，要么在当前函数内的临时槽中存取，确保 `if`/`loop` 结果可以通过共享槽实现分支合并。
- 正确处理 `loop`/`while`/`if` 等控制流情况下的 `break`、`continue`、`return`，利用 `node_outcome_state_map` 判断分支是否仍有 `NEXT` 流向，避免生成死块。
- 在 lowering 过程中支持运行时/内建函数与方法（`print`、`println`、`String::from` 等），沿用语义阶段回填的 `FnDecl` 信息和 TypeLowering 的签名。
- 全部实现完成后，为 docs/TODO.md 中提到的样例（`testcases/semantic-2`、`testcases/IR-1`）提供 IR 生成能力，同时补充覆盖性自动化测试和文档记录实现细节。

#### 输入依赖与驱动入口
- `ir::IRModule &` / `ir::IRBuilder &` / `ir::TypeLowering &`：IR 生成的基础设施。IRBuilder 已拥有 `create_alloca/load/store/gep/icmp/call/br/ret` 等原语，本阶段将继续复用。若需要对聚合体进行整体复制，将在 IRBuilder 中新增 `create_memcpy(IRValue_ptr dst, IRValue_ptr src, size_t bytes)`，统一生成 `call void @llvm.memcpy.p0.p0.i32` 的封装，便于数组/结构体赋值。
- 语义检查上下文：
  - `map<size_t, Scope_ptr> &node_scope_map`：定位任意节点所属的作用域，进而查找变量/常量声明。
  - `map<Scope_ptr, Local_Variable_map> &scope_local_variable_map`：记录每个作用域内的 `LetDecl`（含函数形参），用于分配栈槽。
  - `map<size_t, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map`：表达式类型及其是否是可写左值。
  - `map<size_t, OutcomeState> &node_outcome_state_map`：控制流分析结果，用于判断基本块是否终止。
  - `map<size_t, FnDecl_ptr> &call_expr_to_decl_map`：每个函数调用最终绑定到的 `FnDecl`。
  - `map<ConstDecl_ptr, ConstValue_ptr> &const_value_map`：供常量表达式在 IR 中内联或引用。
- 驱动类 `IRGenerator`（`include/ir/IRGen.h`, `src/ir/IRGen.cpp`）负责遍历整个 AST：
  ```cpp
  class IRGenerator {
  public:
      IRGenerator(IRModule &module,
                  IRBuilder &builder,
                  TypeLowering &type_lowering,
                  map<size_t, Scope_ptr> &node_scope_map,
                  map<Scope_ptr, Local_Variable_map> &scope_local_variable_map,
                  map<size_t, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map,
                  map<size_t, OutcomeState> &node_outcome_state_map,
                  map<size_t, FnDecl_ptr> &call_expr_to_decl_map,
                  map<ConstDecl_ptr, ConstValue_ptr> &const_value_map);

      void generate(const std::vector<Item_ptr> &ast_items);
  };
  ```
  - `generate` 先构造 IRGenVisitor，然后对于每个 item 都用 vistor 去跑即可。

#### 函数级上下文
为避免将全局状态塞进 visitor，本阶段引入以下辅助结构：

- `struct FunctionContext`（声明于 `ir_generator.h`，仅在实现文件可见）：由 `IRGenVisitor::visit(FnItem)` 在进入函数体时构造，负责记录该函数的所有 lowering 状态：
  - `FnDecl_ptr decl`：语义阶段的函数声明。
  - `IRFunction_ptr ir_function`：global lowering 创建的 IR 函数对象。
  - `BasicBlock_ptr entry_block` / `BasicBlock_ptr current_block` / `BasicBlock_ptr return_block`：管理 builder 的插入点与统一的返回出口。
  - `IRValue_ptr return_slot`：若函数返回非 `void` 类型，则在入口 `alloca` 一个槽，用于从任意分支写入返回值；返回 `void` 或 `Never` 时保持 `nullptr`。
  - `unordered_map<LetDecl_ptr, IRValue_ptr> local_slots`：将语义阶段的局部声明映射到 `alloca` 结果。形参在 `visit(FnItem)` 之初生成槽并写入此表，函数体中的 `let` 语句在执行时追加。
  - `IRValue_ptr self_slot`：方法/impl 函数中 `SelfExpr` 对应的栈槽，仅在 `receiver_type != NO_RECEIVER` 时有效。
  - `bool block_sealed`：若当前基本块已经通过 `ret/br` 终结，设置为 `true` 并阻止在同一块继续插入指令。

- `struct LoopContext`：
  - `BasicBlock_ptr header_block`（while/loop 条件所在）、`BasicBlock_ptr body_block`、`BasicBlock_ptr continue_target`、`BasicBlock_ptr break_target`。
  - `IRValue_ptr break_slot`：只有 `loop` 表达式且结果被使用时才会分配，与 let/if 共享统一的“目的槽”机制。
- `RealType_ptr result_type`：`loop { break expr; }` 推断出的类型，供 `TypeLowering` 把 break 值映射成 IRType。
- `unordered_map<size_t, IRValue_ptr> expr_value_map_`：IRGenVisitor 级别的缓存，记录每个表达式（按 `NodeId`）生成的寄存器值或指针值，复用时无需重新生成。
- `unordered_map<size_t, IRValue_ptr> expr_address_map_`：同样挂在 visitor 上，左值或需要地址的表达式在求值时把“写入位置”写入此表，赋值/引用时直接取用。

#### 辅助工具
- `ensure_current_insertion()`：所有需要发射 IR 指令的 `visit` 在 `store/load/br` 等操作前都调用此函数，确保 `ctx.current_block` 非空并同步到 `IRBuilder` 的插入点。这样可避免分支/循环更改插入点后把指令插到错误的基本块里。
- `IRValue_ptr get_rvalue(size_t node_id)` / `IRValue_ptr get_lvalue(size_t node_id)`：统一的寄存器/地址查询入口：
  - `get_rvalue` 优先从 `expr_value_map` 取右值，若没有缓存则根据 `expr_address_map` 里的真实地址执行一次 `load` 并返回；这意味着每个表达式都会产生一个寄存器值，父节点在需要时可以随时读取。
  - `get_lvalue` 只返回真实可寻址表达式的地址（identifier/field/index/deref 等在各自的 `visit` 中写入），如果缺失则直接抛错，帮助我们尽早发现漏填地址的节点。右值引用仍由 `&expr` 等语法在自身 `visit` 中显式分配临时槽并把地址写入 `expr_address_map`，而不是在 `get_lvalue` 内部构造副本。


#### AST 遍历策略
- IR 生成阶段会对整个 AST 再跑一遍，与语义阶段一样复用 visitor 体系，**而不是**只挑函数节点。这样可以保持与语义层一致的结构，对于 `ConstItem`、`StructItem`、`ImplItem` 等非函数节点，只需在对应的 `visit` 中直接返回即可。
- `IRGenVisitor` 仍然以“当前函数”为单位生成 IR。当遍历到 `FnItem` 时，会实例化 `FunctionContext` 并执行函数体 lowering；其余节点（例如 const、struct、impl）在 visitor 中被跳过。

#### IRGenVisitor
`IRGenVisitor` 继承 `AST_Walker`，但真正生成 IR 的只有 `FnItem` 相关逻辑，其余 `Item` 只负责维持遍历序。主要接口如下：
```cpp
class IRGenVisitor : public AST_Walker {
public:
    IRGenVisitor(FunctionContext &fn_ctx,
                 IRBuilder &builder,
                 TypeLowering &type_lowering,
                 map<size_t, Scope_ptr> &node_scope_map,
                 map<size_t, pair<RealType_ptr, PlaceKind>> &node_type_and_place_kind_map,
                 map<size_t, OutcomeState> &node_outcome_state_map,
                 map<size_t, FnDecl_ptr> &call_expr_to_decl_map,
                 map<ConstDecl_ptr, ConstValue_ptr> &const_value_map);

    void visit(LetStmt &node) override;
    void visit(ExprStmt &node) override;
    void visit(ReturnExpr &node) override;
    void visit(BreakExpr &node) override;
    void visit(ContinueExpr &node) override;
    void visit(BinaryExpr &node) override;
    void visit(UnaryExpr &node) override;
    void visit(CallExpr &node) override;
    void visit(IfExpr &node) override;
    void visit(WhileExpr &node) override;
    void visit(LoopExpr &node) override;
    void visit(BlockExpr &node) override;
    void visit(StructExpr &node) override;
    void visit(ArrayExpr &node) override;
    void visit(RepeatArrayExpr &node) override;
    void visit(FieldExpr &node) override;
    void visit(IndexExpr &node) override;
    void visit(IdentifierExpr &node) override;
    void visit(LiteralExpr &node) override;
    // 其余表达式直接在对应的 visit 中生成 IR（无需额外 helper）。

private:
    IRValue_ptr ensure_slot_for_decl(LetDecl_ptr decl);
    void branch_if_needed(BasicBlock_ptr target);
    bool current_block_has_next(size_t node_id) const;
    // 右值采用“现用现取”的策略：如果 expr_value_map 中已有缓存可直接返回；
    // 否则从 expr_address_map[node] 拿到地址后再 load，避免左值修改后返回陈旧寄存器。
    IRValue_ptr get_rvalue(size_t node_id);
    // 左值默认读取 expr_address_map[node]。若缓存缺失（例如访问了 rvalue 但此刻需要引用），
    // 则根据 `node_type_and_place_kind_map` 推断类型，现场为该表达式分配临时 `alloca` 或生成 `&const`，并写回 expr_address_map。
    IRValue_ptr get_lvalue(size_t node_id);
};
```
关键行为：
- `visit(FnItem)` 负责完成整个函数的初始化：创建 `entry/return` 块；遍历 `FnDecl::parameter_let_decls` 调用 `ensure_slot_for_decl` 建立栈槽；将 `ir_function->params()` 的初始值写入局部槽；若返回类型非 `void`，在入口 `alloca` `return_slot`；若 `is_main`，将返回类型强制设为 `i32` 并准备处理 `exit`。
- **表达式访问信约**：
  1. `visit(IdentifierExpr)`、`visit(FieldExpr)`、`visit(IndexExpr)` 等左值表达式在访问时把地址写入 `expr_address_map`；若上层请求右值，则在当下 `load` 并把寄存器写入 `expr_value_map`。读取某个表达式的结果时应优先查 `expr_value_map`（右值），若缺失再从 `expr_address_map` 获取地址并现用现 load。
  2. 若表达式为 `RealTypeKind::NEVER` 或 `OutcomeState` 不含 `NEXT`，在对应的 visit 中立即标记“当前块终止”，阻止继续生成指令。
  3. 父节点通过 `get_rvalue`/`get_lvalue` 主动读取子表达式结果，再自行决定是否把结果写入特定槽位；IRGen 不再维护全局 `current_result_slot`。
  4. 纯右值表达式（算术、函数调用等）在 visit 完成后把寄存器写入 `expr_value_map`，供上层复用。
- `ensure_slot_for_decl`：若 `LetDecl` 尚未映射到 `alloca`，则调用 `TypeLowering::lower(decl->let_type)` 并在入口块创建栈槽。所有局部槽均在入口块分配，符合 LLVM 的 `mem2reg` 预期。
- `current_block_has_next(node_id)` 查询 `node_outcome_state_map[node_id]` 中是否含 `OutcomeType::NEXT`；若缺失，则当前块在语义上已经终止（return/break），visitor 应将 `current_block` 标记为终结状态，阻止追加指令。

#### visit 行为速查
- **FnItem**：创建/清空 `entry` 与 `return_block`，为全部 `parameter_let_decls` 分配栈槽并 `store` 形参值，`alloca` `return_slot`（若返回非 void），处理 main/exit 特判，然后继续访问函数体 `BlockExpr`。
- **LetStmt**：确保目标 `LetDecl` 已有 `alloca`。若有初始化表达式则先访问该表达式、通过 `get_rvalue` 拿到寄存器，再写入局部槽；语义阶段已保证类型匹配，因此无需额外 result slot。
- **ExprStmt**：访问表达式；若 `OutcomeState` 无 `NEXT`，立刻把 `current_block` 置空；若语句末尾**没有**分号且表达式结果类型不是 `()`/`Never`，则把该寄存器结果写入 `expr_value_map[node.NodeId]`，这样父节点（例如 Block 的尾随表达式）可以继续使用。
- **ReturnExpr**：若带值则写入 `return_slot`，随后 `br return_block` 并设置 `block_sealed = true`。
- **BreakExpr/ContinueExpr**：Break 将值写入 `LoopContext.break_slot`（若存在）并 `br break_target`；Continue 直接 `br continue_target`。
- **BlockExpr / 其它表达式返回值**：函数隐式返回、`if`/`loop` 结果等场景由 `get_rvalue(node_id)` 取寄存器值再写回父节点指定的槽位，完全由父节点掌控。只有 `ReturnExpr`、`BreakExpr` 等语义上固定的终止语句会直接写入 `FunctionContext::return_slot` 或 `LoopContext::break_slot`。
- **BinaryExpr**：
  - 算术/位运算：读取左右寄存器，调用 `IRBuilder::create_*` 生成 `%tmp` 写入 `expr_value_map`。
  - 比较：同样调用 `create_compare`，结果为 `i1`。
  - 赋值/复合赋值：读取左值地址（`expr_address_map`），必要时 `load` 原值，与右值组合后写回地址并更新 `expr_value_map[node]`。
  - `&&`/`||`：构建 `lhs_block`/`rhs_block`/合流块，按短路规则跳转；为该表达式单独 `alloca` 一个布尔槽，分支将结果写入后在合流处 `load` 并放入 `expr_value_map[node]`。
- **UnaryExpr**：`NEG/NOT` 直接对右值寄存器做算术；`REF/REF_MUT` 将右值地址写入 `expr_value_map[node]`；`DEREF` 把右值指针写入 `expr_address_map[node]`。
- **CallExpr**：根据 `call_expr_to_decl_map` 找到目标 `FnDecl`，收集实参寄存器后调用 `create_call`。  
  若 `callee` 是 `FieldExpr` 且 `FnDecl::receiver_type != NO_RECEIVER`，则先获取 base 表达式的地址/右值并作为 `self` 插到参数列表最前面：`&mut self` 传地址、`&self` 传地址并标记只读、`self` 传右值。  
  main 中的 `exit` 特判为“写入 `return_slot` + `br return_block`”。
- **IfExpr**：若表达式结果会被后续使用，则在当前函数中 `alloca` 临时槽承载该值；`then/else` 块根据 OutcomeState 选择性写入该槽，merge 处 `load` 并写入 `expr_value_map[node]`，父节点随后用 `get_rvalue` 读取即可。
- **BlockExpr**：顺序访问语句；若存在尾随表达式则访问它、把寄存器写入 `expr_value_map[node]`，由父节点自行决定是否 `store` 到其它地址。
- **WhileExpr/LoopExpr**：创建 `cond`/`body`/`exit`（loop 的 `cond` 直接跳 body），更新 `LoopContext`，在 cond 中生成比较并据此跳转；loop 若需要返回值则在 `break_slot` 分配槽，break 时写入。
- **StructExpr/ArrayExpr/RepeatArrayExpr**：若存在目标地址则就地写入，否则 `alloca` 临时槽，再逐字段/元素访问并写入；Repeat array 在写入首元素后用循环复制其余元素。
- **FieldExpr/IndexExpr/IdentifierExpr/SelfExpr**：把可寻址结果写入 `expr_address_map[node]`；若需要右值则 `load`。字段/索引使用 `create_gep` 计算偏移，SelfExpr 直接返回 `self_slot`。
- **LiteralExpr**：生成 `ConstantValue` 填入 `expr_value_map[node]`；字符串通过 `create_string_literal` 生成全局字面量。
- **ConstDecl 引用**：数组常量使用 global lowering 的 `GlobalValue`；标量常量直接转换为 `ConstantValue`。

#### 语句 lowering
- **LetStmt**：遇到声明时立即调用 `ensure_slot_for_decl` 为该 `LetDecl` 分配 `alloca` 并记录到 `local_slots`；若存在初始化器，则先访问该表达式，把结果寄存器写回 `expr_value_map`，随后 `store` 到刚分配的槽即可（不需要额外的 `current_result_slot_` 传递）。
- **赋值表达式**：`BinaryExpr` 的 `ASSIGN`/`*_ASSIGN` 通过 `lower_place_expr(node.left)` 获取地址，然后将右值（或算术结果）写入该地址。对于 `+=` 等运算，先 `load` 左值，再使用算术 helper（`create_add` 等）计算后 `store`。
- **ExprStmt**：访问内部表达式，让其在 `expr_value_map`/`expr_address_map` 中生成相应的 IRValue；若语句末尾没有分号且结果类型不是 `()`/`Never`，则把寄存器写入 `expr_value_map[node]`，否则忽略该结果（除非子表达式发散，此时 `current_block` 将被置空）。
- **ItemStmt**：函数体中的 `Item` 在语义阶段已展开，此处直接忽略或转交 `AST_Walker`（无副作用）。

#### 控制流与块
- **BlockExpr**：按顺序处理内部语句。若 `node.tail_statement` 存在，则访问尾随表达式并把其结果写入 `expr_value_map[node]`；遍历途中若某条语句的 OutcomeState 不含 `NEXT`，立刻停止并把 `current_block` 设为 `nullptr`。
- **IfExpr**：
  1. 若 `if` 表达式的结果会被使用（类型既非 `()` 也非 `Never`），则在当前函数里申请一个临时 `alloca` 作为分支共享的结果槽。
  2. 创建 `then_block`、`else_block`（若缺省则直接跳到 merge）与“合流块”。这个合流块可以是专门的 `if.merge`，也可以复用下一个自然块（例如 `loop.cont`），只要所有 `NEXT` 分支都跳到同一个 label 即可；实现上只需在 `FunctionContext` 里记录一个 `merge_target` 指针。
  3. 对每个分支分别设置 `current_block` 与插入点。若 `node_outcome_state_map` 表明某分支不会落入 `NEXT`，则无需显式跳转至合流块。
  4. 只有 `OutcomeState` 包含 `NEXT` 的分支才会把该槽写满并在末尾 `br merge_target`。
  5. 合流块若槽非空，则对其执行 `load` 并写入 `expr_value_map[node]`，父节点后续通过 `get_rvalue` 获取寄存器即可。
- **WhileExpr**：构建 `cond_block`、`body_block`、`exit_block`。`LoopContext` 栈在进入 `body` 前压入 {continue_target=cond_block, break_target=exit_block}。条件表达式只在 `cond_block` 中执行一次，若 `OutcomeState` 表示条件发散，则直接落入 exit。
- **LoopExpr**：类似 while，但无条件回到 body。若 `node.must_return_unit == false` 且外部需要其结果，则 `LoopContext.break_slot` 指向一块 `alloca`（即 loop 表达式的返回地址），每个 `break expr` 先写入此槽再跳转到 `break_target`。若 loop 没有 `break` 且推断类型为 `Never`，则 `lower_expr` 返回 `is_never = true`。
- **Break/Continue**：读取 `loop_stack.back()`，对 `break`：
  - 若 `break_value` 非空，使用 `LoopContext.break_slot` 写入结果；若 slot 缺失但语义上需要返回，则抛错。
  - 之后调用 `builder.create_br(loop_ctx.break_target)` 并将当前块标记为终结。
  `continue` 则直接跳到 `loop_ctx.continue_target`。
- **ReturnExpr**：若表达式存在，则写入函数级 `return_slot`；随后 `br return_block` 并终结当前块。返回类型为 `Never` 时可直接 `ret void`。

#### 左值与聚合类型
- **IdentifierExpr / PathExpr**：
  - 通过 `node_scope_map` + `scope_local_variable_map` 查找 `LetDecl`。若找到则直接返回 `alloca` 指针。
  - 若未找到，视情况处理 `ConstDecl`（利用 `const_value_map` + `TypeLowering::lower_const` 得到 `ConstantValue` 或数组全局符号）与 `FnDecl`（函数指针调用场景）。
  - 对关联常量 `Type::CONST`，借助 `Scope::type_namespace` 定位结构体，再取其 `associated_const`。
- **FieldExpr**：
  - 对于结构体左值，先调用 `lower_place_expr(node.base)` 获取指针，再根据 `StructDecl::field_order` 计算字段序号，构建 `gep` (`{0, field_idx}`)。
  - 若 base 为 `&str`/`String` 这类命名结构体，同样根据布局缓存偏移。
- **IndexExpr**：两类情况：
  1. 基础是数组值：`lower_place_expr(base)` 后，用 `builder.create_gep` 连续应用 `[0, index]`。
  2. 基础是 slice/指针：index 以 `i32` 保存，调用 `create_gep`（element_type = 数组元素类型）。
- **StructExpr / ArrayExpr / RepeatArrayExpr**：
  - 聚合构造统一遵循“调用方决定目标地址”的规则：若上层提供了地址（`let foo = Struct { ... }`），就在该地址上就地写入；否则先在本函数里 `alloca` 一个临时槽再写入，最后 `load` 成寄存器值。
  - `StructExpr` 遍历 `node.fields`，对每个字段调用 `lower_expr(field_expr, /*dest=*/field_gep_address)`。
  - `ArrayExpr` 与 `RepeatArrayExpr` 通过 `create_gep` 定位 `[0, element_index]`，重复写入。Repeat 里的元素表达式只计算一次，随后通过简单的 `for` 循环把该值写到其余位置（第一版暂不调用 `llvm.memcpy`）。
- **结构体/数组赋值（临时策略）**：第一版直接把整个聚合视为一个 SSA 值：
  - `lower_place_expr(lhs)` 获取目标地址；`lower_expr(rhs)` 若返回地址则直接 `load struct_ty, ptr %rhs`。
  - 将得到的聚合值 `store struct_ty %loaded, ptr %lhs_addr`。数组也沿用相同方式。
  - 这种“整体 load → store” 的表达方式简单直观，后续如需规避 padding/`undef` 问题，再替换为 `llvm.memcpy` 或逐字段拷贝。
- **ConstDecl 引用**：当 `IdentifierExpr` 绑定到 `ConstDecl` 时：
  - 若是数组常量，则 global lowering 已经创建了 `@const.NAME`，直接返回该 `GlobalValue`.
  - 若为标量，使用 `const_value_map` + `TypeLowering::lower_const` 得到 `ConstantValue`，无需额外 `load`。

#### 表达式分类策略
- **LiteralExpr**：根据 `LiteralType` 生成 `ir::ConstantValue`。字符串字面量借助 `IRBuilder::create_string_literal` 分配 `[len+1 x i8]` 全局，返回 `{ ptr, i32 }` 聚合。
- **UnaryExpr**：
  - `NEG`/`NOT`：使用 `create_sub` / `create_xor`。
  - `REF`/`REF_MUT`：调用 `lower_place_expr`，直接返回地址（并将表达式类型置为指针）。
  - `DEREF`：右操作数必须是指针类型；结果为该指针的 pointee 地址。
- **BinaryExpr**：
  - 算术/位运算直接调用 `IRBuilder` 对应 helper。
  - `AND_AND`、`OR_OR` 使用短路语义：创建 `lhs_block`/`rhs_block`/最终合流块，并在本函数内分配临时槽承载布尔结果；merge 处 `load` 后写入 `expr_value_map[node]`。
  - 比较运算使用 `create_compare` + 语义阶段提供的类型信息区分有符/无符（`RealTypeKind::I32/ISIZE` → signed，`U32/USIZE` → unsigned）。
  - 赋值类运算参考“语句 lowering”部分。
- **CallExpr**：
  - 先解析 `call_expr_to_decl_map[node.NodeId]` 获取目标 `FnDecl`，并从中读取 `name`（global lowering 已加后缀）以调用 `builder.create_call`.
  - 若 `callee` 是 `FieldExpr`，意味着方法调用：在参数列表前注入 `self`（按 `receiver_type` 区分值/引用/可变引用），传参顺序与 `FnDecl::parameters` 一致。
  - `PathExpr` 代表关联函数通过 `Type::func` 被调用，直接使用语义阶段解析的 `FnDecl`。
  - 对内建函数（如 `print/println/exit`）不需特判，它们在语义阶段就以普通 `FnDecl` 注册，IR 侧只要调用对应符号即可。
  - main 函数里的 `exit(code)` 需要特殊处理：直接把 `code` 写入 `return_slot` 并跳转到 `return_block`，不再真正发出 `call @exit`。语义阶段限定 `exit` 只能在 main 中出现，因此其他函数仍按普通 `call` 处理。
- **CastExpr**：根据 `node_type_and_place_kind_map` 判定源/目标类型。整数之间通过 `create_zext`/`create_sext`/`create_trunc`（必要时在 IRBuilder 中新增），引用相关的 cast 应在语义阶段禁止，这里只需 assert。

#### 与控制流分析的集成
- 任何语句或表达式若对应的 `OutcomeState` 不含 `OutcomeType::NEXT`，都意味着当前基本块已经自然终止。`IRGenVisitor` 会将 `FunctionContext::block_sealed` 标记为 true，并禁止再次发射 `br`/`ret`。
- 对 `Never` 类型（TypeLowering → `void`）的表达式，`lower_expr` 会返回 `is_never = true`，使调用方即刻停止生成后续代码。
- 当一个 `if` 或 `loop` 的所有分支都缺失 `NEXT` 时，将不会创建 `merge_block`，避免生成死块、空跳转。

#### 测试计划
1. **单元测试：结构级断言**  
   新增 `test/IRBuilder/test_stmt_expr_lowering.cpp`，手动拼装几个极简 AST（对应 `fn main`、`let`、`if`、`loop`），跑完整个语义阶段与 IR 生成功能，断言 `IRModule::serialize()` 输出包含预期的 `alloca`、`br`、`store` 与命名规则。重点覆盖：
   - `let`/赋值/引用，验证栈槽分配与 `load/store`。
   - `if`/`loop`/`break value` 控制流及共享槽。
   - `StructExpr`/`FieldExpr`，验证 `gep` 下标。
   - `&&`/`||` 的短路结构。
2. **端到端样例：执行验证（暂无 runtime）**  
   写若干不依赖任何内置函数/方法的极简程序（仅用算术、if/while/loop、数组/结构体、`exit`），放在 `test/IRGen/` 目录（当前已有 `arithmetic_combo.rx`、`loop_sum.rx`、`const_usage.rx`、`reference_ops.rx` 等示例）。每个案例都以 `exit(expected_code)` 表示“逻辑是否通过”：例如 `exit(0)` 代表通过，`exit(42)` 代表该测试预期的返回值。测试脚本顺序执行 `Lexer -> Parser -> Semantic_Checker -> IRGenerator`，把 IR 文本喂给 `clang -x ir -`（或 `clang -c -o a.out -`）生成可执行文件，再运行并检查进程退出码是否与案例描述一致，无需额外的 stdout 检查。  
   - 所有例子需遵守语言语法：`exit` 只能在 main 末尾调用一次；`if/while` 条件必须写括号；不可使用 `print`/`String::from` 等 runtime 依赖。
   - 案例涵盖基础算术、比较、while/loop/break/continue、结构体/数组访问、短路逻辑等；若需要观察中间结果，就在 main 内累加到某个变量，最后由 `exit(acc)` 返回。
3. **回归样例**  
   复用 `testcases/semantic-2` 里原有的语义测试，增加一个新的 CI 入口（例如 `scripts/run_ir_stmt_tests.sh`）只生成 IR 并检查是否与基准文本一致，确保未来修改不会破坏命名/布局。

测试输出以 `EXPECT_EQ` 形式比较文本，或读取运行脚本的退出码与标准输出。所有新测试在 CI 中默认开启，确保“函数体、语句与表达式 lowering” 具备回归保障。

#### 参考示例

1. **最简单的函数**
   ```rust
   fn main() -> i32 { 42 }
   ```
   生成的 IR（省略 runtime）：
   ```llvm
   define i32 @main() {
   entry:
     %retval = alloca i32
     store i32 42, ptr %retval
     br label %return
   return:
     %0 = load i32, ptr %retval
     ret i32 %0
   }
   ```

2. **分支结构**
   ```rust
   fn select(a: i32, b: i32, cond: bool) -> i32 {
       if cond { a } else { b }
   }
   ```
   核心 IR：
   ```llvm
   define i32 @select(i32 %arg.0, i32 %arg.1, i1 %arg.2) {
   entry:
     %ret.slot = alloca i32
     br i1 %arg.2, label %then, label %else
   then:
     store i32 %arg.0, ptr %ret.slot
     br label %merge
   else:
     store i32 %arg.1, ptr %ret.slot
     br label %merge
   merge:
     %0 = load i32, ptr %ret.slot
     ret i32 %0
   }
   ```

3. **loop + break/continue**
   ```rust
   fn sum_to(limit: i32) -> i32 {
       let mut acc = 0;
       let mut i = 0;
       loop {
           if i == limit { break acc; } else { }
           i = i + 1;
           acc = acc + i;
       }
   }
   ```
   关键 IR 片段（`if` 仍会生成 `then/else` 两个分支；then 分支直接 `break`，else 分支继续执行；`continue` 可以直接 `br loop.body`。每个 `if/else` 自己维护 merge 块，循环结构只包含 `loop.body` 与 `loop.break` 两种出口。）：
   ```llvm
   entry:
     %acc = alloca i32
     %i = alloca i32
     store i32 0, ptr %acc
     store i32 0, ptr %i
     br label %loop.body
   loop.body:
     %cur.i = load i32, ptr %i
     %cmp = icmp eq i32 %cur.i, %arg.0
     br i1 %cmp, label %if.then, label %if.else
   if.then:
     %acc.val = load i32, ptr %acc
     store i32 %acc.val, ptr %ret.slot
     br label %loop.break
   if.else:
     br label %if.merge
   loop.break:
     br label %return
   if.merge:
     %next.i = add i32 %cur.i, 1
     store i32 %next.i, ptr %i
     %acc.next = load i32, ptr %acc
     %sum = add i32 %acc.next, %next.i
     store i32 %sum, ptr %acc
     br label %loop.body          ; continue = 回到 loop.body
   ```
   通过 `loop.break` 与 `loop.cont` 基本块显式地表现 `loop` 的 break 值与继续执行，从而保持与 Rust 控制流一致。

4. **while 循环**
   ```rust
   fn count(mut i: i32) -> i32 {
       let mut acc = 0;
       while i > 0 {
           acc = acc + i;
           i = i - 1;
       }
       acc
   }
   ```
   生成 IR：
   ```llvm
   entry:
     %acc = alloca i32
     store i32 0, ptr %acc
     br label %while.cond
   while.cond:
     %cur.i = load i32, ptr %arg.0
     %cond = icmp sgt i32 %cur.i, 0
     br i1 %cond, label %while.body, label %while.end
   while.body:
     %acc.val = load i32, ptr %acc
     %sum = add i32 %acc.val, %cur.i
     store i32 %sum, ptr %acc
     %next.i = add i32 %cur.i, -1
     store i32 %next.i, ptr %arg.0
     br label %while.cond
   while.end:
     %result = load i32, ptr %acc
     ret i32 %result
   ```
