IRGen 后续优化 TODO：

- 为所有 aggregate 类型（结构体/数组）提供 `size_in_bytes` 计算接口，便于统一生成 `memcpy/memset`。（已完成）
- 在 IRGen 中用 `llvm.memset`/`llvm.memcpy` 取代逐元素 `store`，例如零初始化 `[N x T]` 或结构体字面量时直接调用 `memset` 或从全局常量 `memcpy`。（已完成）
- 调整函数签名：当返回值是 `[T; N]` 或结构体时，改为传入 caller 分配的缓冲区（隐式指针参数），函数内部写入该缓冲区后返回 `()`。
- 调用端同步更新：调用 aggregate-return 函数前先 `alloca` 出结果空间，传指针，必要时用 `memcpy` 将结果写到目标变量。
