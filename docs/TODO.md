实现 PLAN.md 中的第五步：**整合编译驱动与外部接口**  

我现在需要实现 builtin 的函数，包括：

- fn print(s: &str) -> ()
- fn println(s: &str) -> ()
- fn printInt(n: i32) -> ()
- fn printlnInt(n: i32) -> ()
- fn getString() -> String
- fn getInt() -> i32
- exit(code: i32) -> !

注意，exit 已经在 IRGen 里面被特殊处理过了，所以不用生成。

Builtin methods:

- fn to_string(&self) -> String

available for u32, usize

- fn as_str(&self) -> &str
- fn as_mut_str(&mut self) -> &mut str

available for String

- fn len(&self) -> usize

available for &str, String, [T; N]

关于 [T; N] 这样子的数组，由于都是编译期确定的，所以计划在 IR 期间遇到这个直接返回常量。未实现。

其他两个，就参考 Str 和 String 的布局，就可以分别实现返回值。

- fn append(&mut self, s: &str) -> ()

associated functions for String:

- fn from(&str) -> String
- fn from(&mut str) -> String

此外，我 TypeLowering 也并没有声明所有需要的函数。所以我还需要在 TypeLowering 里面补充声明这些内建函数。我觉得不应该一下子全部声明，这样子太冗长了，而是用到了再声明。