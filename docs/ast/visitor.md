### visitor 模块

该文件定义语法树访问器体系，所有节点均通过继承的 `accept(AST_visitor &)` 方法与访问器交互。核心接口 `AST_visitor` 要求为每一种表达式、语句、类型与模式节点提供重载，语义分析阶段可按需继承并覆盖具体处理逻辑。

#### AST_visitor
- 纯虚基类，声明对 30 余种 AST 节点的 `visit` 方法。实现者通常保持轻量逻辑，仅在关心的节点上执行动作，忽略的节点可以维持空实现。
- 与 `AST_Node::accept()` 协作时，节点会把自身引用传入对应的 `visit` 函数，实现双分派。

#### AST_Walker
- 默认递归访问器，继承自 `AST_visitor`，在 `visit` 中负责遍历子节点，未做任何额外处理。
- 对于叶子节点（字面量、标识符、`UnitExpr` 等）返回；对复合节点递归调用 `accept`，例如 `BinaryExpr` 依次访问 `left`、`right`，`BlockExpr` 遍历语句列表并处理尾随表达式。
- 复杂语句有特殊处理：`BlockExpr` 会根据 `tail_statement` 判断是否存在尾随表达式；`ArrayType` 同时访问元素类型与长度表达式。
- 适合作为其他访问器的基类，被广泛用于作用域构建、类型检查等流程以减少重复代码。

#### AST_Printer
- 继承自 `AST_Walker`，借助 `depth` 记录当前缩进层，并在访问节点前输出节点类型与关键字段，形成可读性较强的树状结构调试输出。
- 典型重写示例：
  * `visit(BinaryExpr &)`：打印运算符后再递归子节点，缩进加一。
  * `visit(FnItem &)`：展示函数名、参数列表、返回类型，再遍历函数体。
- 通过保持 `depth` 自增/自减，确保任意嵌套结构都能正确反映在输出中。

#### ASTIdGenerator
- 继承自 `AST_Walker`，在访问每个节点前递增 `current_id` 并写入 `node.NodeId`，随后委托基类完成子节点遍历。
- `Parser::parse()` 在生成语法树后立即运行该访问器，对整棵树进行一次 DFS，以保证 `NodeId` 的唯一性与稳定顺序。该编号被语义分析阶段用作哈希键。

#### 实用建议
- 若仅需遍历子节点，可直接复用 `AST_Walker`（或在派生类中调用 `AST_Walker::visit(node)`）避免手写递归。
- 如果访问器需要维护上下文（如作用域栈、类型映射），在重写的 `visit` 中先处理当前节点，再调用 `AST_Walker::visit(node)` 访问子节点是一种常见模式。
