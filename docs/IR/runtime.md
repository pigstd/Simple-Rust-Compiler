### Runtime 模块设计

本节描述 `runtime.c` 中实现的内建函数、字符串/数组工具与其 LLVM 侧 ABI 细节。IRGen 生成的 `.ll` 需要在链接或解释执行时与该 runtime 一同提供，否则 `print`、`String::from` 等符号会缺失。所有约定均基于 LLVM 15+ 的 opaque pointer 语法。

#### 数据布局总览
- `&str`：`%Str = type { ptr, i32 }`，字段依次为指向 UTF-8 字节序列的 `ptr` 与长度（字节数）。数据不保证以 `\0` 结尾，使用者需依赖长度。
- `String`：`%String = type { ptr, i32, i32 }`，字段依次为堆缓冲区指针、当前长度、容量（字节数）。运行时所有写入必须保持 `len <= capacity`，且在需要与 libc 交互时负责补写结尾 `\0`。
- `[T; N]`：在 runtime 内不会动态创建定长数组。`len()` 针对数组的内建直接由 IRGen 生成常量，但 runtime 中的其他函数（例如 `append`、`String::from`）可以假设编译器提供的布局。

TypeLowering 在初始化时会调用 `declare_builtin_string_types()` 注册 `%Str` 与 `%String`，runtime 的符号实现必须与该结构保持一致；若未来扩充字段，需同步更新 TypeLowering 与 IRBuilder 文档。

#### 顶层内建函数
| 函数 | LLVM 符号 | 说明 |
| --- | --- | --- |
| `fn print(s: &str)` | `declare void @print(%Str)` | 将 UTF-8 切片输出到 stdout，不自动换行。runtime 可通过 `printf("%.*s", len, ptr)` 或手动构建带 `\0` 的缓冲交给 `printf`。 |
| `fn println(s: &str)` | `declare void @println(%Str)` | 等同于 `print` 后追加 `'\n'`，可复用 `print` 逻辑。 |
| `fn printInt(n: i32)` | `declare void @printInt(i32)` | 以十进制打印整数，不换行。可用 `printf("%d", n)` 或 `sprintf` 到临时缓冲后再输出。 |
| `fn printlnInt(n: i32)` | `declare void @printlnInt(i32)` | `printInt` 后追加 `'\n'`。 |
| `fn getString() -> String` | `declare %String @getString()` | 通过循环调用 `getchar()` 读取一行 UTF-8，去掉行尾 `'\n'`，返回新分配的 `String`。需处理 EOF（返回空字符串）。 |
| `fn getInt() -> i32` | `declare i32 @getInt()` | 调用 `scanf("%d", &tmp)` 或 `sscanf` 加自定义缓冲读取十进制整数，忽略前导空白。遇到非法输入可返回 0 或继续重试（实现需保证不会死循环）。 |
| `fn exit(code: i32) -> !` | 不由 runtime 提供 | IRGen 已特殊处理，直接生成 `ret` 或调用宿主 `exit`，runtime 无需实现同名符号。 |

所有函数以 C 调用约定暴露，无额外 mangling；实现时务必使用 `extern "C"`（C++）或 `#[no_mangle] extern "C"`（Rust）导出。

#### String/Str 相关方法
##### `fn String::from(&str) -> String`
- LLVM 符号：`declare %String @String_from(%Str)`
- 行为：为输入切片分配新的 `String`，容量可以等于长度或进行倍增。必须复制原字节，确保原始切片释放后仍可使用。

##### `fn String::from(&mut str) -> String`
- 约定与 `&str` 版本相同，但调用方会传 `ptr` 指向可写缓冲；runtime 可以直接复用上述实现。

##### `fn String::append(&mut self, s: &str) -> ()`
- LLVM 符号：`declare void @String_append(ptr %self, %Str)`
- 语义：将 `s` 追加到 `self` 尾部。若容量不足，需 `realloc` 或重新分配新缓冲（常见策略：容量翻倍或 `len + s.len`）。更新 `len` 与 `capacity` 字段；为兼容 C API，可在 `data[len]` 处写入 `'\0'`。

##### `fn String::as_str(&self) -> &str`
- LLVM 符号：`declare %Str @String_as_str(ptr %self)`
- 返回 `(data, len)`，不复制数据。调用方不会修改返回切片。

##### `fn String::as_mut_str(&mut self) -> &mut str`
- LLVM 符号：`declare %Str @String_as_mut_str(ptr %self)`
- 返回 `(data, len)`，允许调用方就地修改已有字符，但不得超出 `len`。

##### `fn String::len(&self) -> usize`
- 可与 `&str` 共用实现：`declare i32 @Str_len(%Str)` / `declare i32 @String_len(ptr %self)`。IRGen 对 `[T; N]` 的 `len` 会直接折叠为常量，因此 runtime 不需处理数组 `len`。

##### `fn u32::to_string(&self) -> String` / `fn usize::to_string(&self) -> String`
- LLVM 符号：`declare %String @u32_to_string(i32)`、`declare %String @usize_to_string(i32)`
- 按十进制无符号格式化，返回新的 `String`。实现可先用 `sprintf` 写入临时缓冲并根据 `strlen` 结果分配目标 `String`。

#### 内存管理策略
1. **分配函数**：运行时统一使用 `malloc`/`realloc`/`free`/`calloc` 并辅以 `memcpy`/`memmove`/`memset` 进行内存管理；若目标平台需要自定义分配器，可在 runtime 层封装。
2. **String 生命周期**：编译器当前不会自动调用析构函数，因此 runtime 不需要暴露 `String_drop`。调用者应把 `String` 当作永远存在的值（直到进程退出）。未来若加入显式 drop，再扩展文档。
3. **线程安全**：运行时时刻假设单线程；若未来支持多线程，需要引入锁或原子操作。

#### TypeLowering 与 runtime 的衔接
- `TypeLowering::declare_builtin_string_types()` 必须在编译流程早期调用，以便 runtime 函数能引用 `%Str`/`%String`。当运行时新增其它结构（例如 `Vec`），也需要在 TypeLowering 中注册对应布局，并在 `IRModule::ensure_runtime_builtins()` 内声明符号。
- 对于 `len(&[T; N])` 等完全可静态求值的内建，优先由 IRGen 直接生成常量，避免 runtime 调用。本文档只描述 runtime 真正需要实现的符号。

#### 构建与交付
1. 将 runtime 源文件集中放在仓库根目录的 `runtime/` 文件夹：至少包含 `runtime.c`（实现所有符号）与 `runtime.ll`（通过 C 代码生成的 LLVM IR）。目录内可再添加 `README` 说明如何重新生成 `.ll`。
2. 使用 `clang -emit-llvm -S runtime/runtime.c -o runtime/runtime.ll` 生成 IR；若需要 `.bc` 可以额外运行 `llvm-as runtime/runtime.ll -o runtime/runtime.bc`。始终以 C 作为语义真源，避免手写 `.ll` 造成偏差。
3. 编译驱动在最终生成的程序 IR 前追加 `runtime.ll`（例如 `cat runtime/runtime.ll program.ll > combined.ll`）或在 `clang` 链接阶段把 `runtime/runtime.ll` 作为额外输入，保证内建符号可解析。
4. CI 建议在 `cmake` 构建结束后检测 `runtime/runtime.ll` 是否比 `runtime/runtime.c` 旧，若是则自动调用 `clang -emit-llvm -S` 生成最新 IR，并把产物复制/符号链接到 `build/runtime.ll`，供 `test/IRGen/run_tests.py` 的驱动脚本链接测试。

#### 调试建议
- 通过 `lli combined.ll` 直接运行，观察 `print`、`append` 等函数行为是否符合预期。
- 若遇到崩溃，可在 runtime 中加入临时的 `printf` 调试信息或使用 AddressSanitizer（`clang -fsanitize=address -g`）编译 runtime 以定位越界。
- 运行 `test/IRGen/run_tests.py` 时，可在脚本中加入 `--dump-ir` 等自定义开关，把最终 IR 导出后手动与 runtime.ll 链接验证。

#### 端到端测试策略
1. **生成程序 IR**：使用 `test/IRGen/run_tests.py` 或自定义 driver 产出目标 `.ll`。推荐为每个 runtime 功能准备一份极小的 `.rx` 样例（如 `print`、`String::append`、`getString`）方便回归。
2. **合并 runtime**：将 `runtime/runtime.ll` 与上述 IR 组合。可直接使用 `cat runtime/runtime.ll program.ll > combined.ll`，或在 `clang`/`lli` 命令中把 `runtime/runtime.ll` 作为额外输入。
3. **驱动执行**：
   - 若测试需要输入，事先准备文本并通过 `subprocess.run(input="...")` 或 shell Here-Doc 传入。
   - 将标准输出/标准错误捕获到内存，编写断言检查是否等于期望字符串（或 exit code）。
4. **脚本封装**：建议新增 `test/runtime/run_tests.py`（或等效路径），其逻辑：
   ```
   cases = [
       {"src": "print_string.rx", "stdin": "", "stdout": "hello", "exit": 0},
       {"src": "string_append.rx", "stdin": "", "stdout": "ok", "exit": 0},
       {"src": "io_roundtrip.rx", "stdin": "abc\n123\n", "stdout": "abc-123", "exit": 0},
   ]
   ```
   - 对每个 case：调用 IRGen driver -> 写入 `.ll` -> 用 `cat runtime.ll program.ll | clang -x ir - -o a.out` 生成可执行文件 -> 运行并核对 stdout/exit。
   - 测试脚本应失败于任何不匹配情况，并输出导致失败的程序与捕获的 IR 片段。
5. **CI 集成**：在 `.github/workflows/ci.yml` 中追加上述 runtime 测试步骤，确保每次提交都验证 runtime + IRGen 的组合。
6. **完整回归**：在自定义 runtime 测试脚本通过后，再运行 `scripts/semantic_testcases_tester -t semantic-2`（或等效命令），确认 `testcases/semantic-2` 下的所有语义/运行时组合测试均能通过，以此作为 Runtime 模块的最终验收。
