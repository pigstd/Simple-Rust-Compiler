### semantic/controlflow 模块

该模块在语义分析第三阶段执行控制流分析，用于检测 `break/continue` 是否处于循环中、判断分支是否返回，以及帮助推断表达式的发散性（diverge）。

#### OutcomeState 与辅助函数
- `OutcomeType`：`NEXT`（正常继续）、`RETURN`、`BREAK`、`CONTINUE`、`DIVERGE` 五种可能的控制流结果。
- `OutcomeState`：`std::bitset<4>` 的别名，用低位存储上述状态（`CONTINUE` 与 `BREAK` 共用下标，见实现）。
- `get_outcome_state(vector<OutcomeType>)`：创建包含指定状态的位集。
- `has_state()`、`sequence_outcome_state()`、`ifelse_outcome_state()`、`while_outcome_state()`、`loop_outcome_state()`：组合多个状态，建模顺序执行、分支与循环的控制流传播规则。

#### ControlFlowVisitor
- 继承自 `AST_Walker`，在遍历每个节点后把该节点的控制流结果记录到 `node_outcome_state_map[NodeId]`。
- 成员 `size_t loop_depth` 追踪当前是否处于循环体内，用来校验 `break` / `continue` 的合法性。
- 关键行为：
  * 基本表达式（字面量、标识符等）标记为 `NEXT`。
  * `BlockExpr` 将内部语句依次通过 `sequence_outcome_state` 合并，尾随表达式也纳入结果。
  * `IfExpr`、`WhileExpr`、`LoopExpr` 分别调用对应的组合函数推导整体控制流。
  * `ReturnExpr` 直接标记为 `RETURN`；`BreakExpr` 要求 `loop_depth > 0`，否则抛出 `CE`。
  * `ContinueExpr` 同样要求在循环内，当前实现将其状态标记为 `BREAK` 位（与 `OutcomeType::BREAK` 共用下标），后续分析通过位集解释。
  * 函数调用会依次合并参数表达式的状态，以捕获其中的控制流影响。

该分析结果在后续类型检查阶段用于判断表达式是否必定返回或可能发散，并辅助 `loop`/`if` 的类型合并逻辑。
