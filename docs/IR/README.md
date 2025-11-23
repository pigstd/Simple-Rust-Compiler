### IR 使用说明

- **输出即 LLVM IR**：IRBuilder 序列化出的 `.ll` 文本完全遵循 LLVM IR 语法，可以直接喂给 `llvm-as`/`lli`/`clang` 等官方工具链继续编译或执行，不存在自定义方言或额外转换步骤。

- **LLVM 版本要求**：本仓库生成的 IR 已全面采用 opaque pointer 语法（即所有指针均输出为 `ptr`，不再携带元素类型）。该语法自 LLVM 15 起才默认启用，因此请使用 LLVM 15 及以上版本的 `llvm-as`/`llc`/`lli`/`clang` 等工具处理输出的 `.ll` 文件；若使用更早版本，需要显式打开 `-opaque-pointers` 才能接受此语法。

- **运行时依赖**：IR 依赖 `runtime.ll`（或其编译产物 `runtime.bc`）中提供的内建函数和字符串/数组操作实现。无论是直接交给 `lli` 解释执行，还是通过 `clang`/`llc` 编译成本地码，都请确保把生成的 IR 与该运行时一同链接，否则诸如 `print`、`String::from` 等符号会缺失。

这份 README 作为 `docs/IR/*` 文档的补充，后续若新增 LLVM 版本要求或 runtime 内容，请同步更新此处。
