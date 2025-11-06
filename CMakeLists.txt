cmake_minimum_required(VERSION 3.22)
project(SimpleRustCompiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- LLVM ----
# 若用 apt 安装了 llvm-18-dev，通常可直接找到；
# 若用官方二进制包，配置时可临时传 -DLLVM_DIR=/path/to/lib/cmake/llvm
find_package(LLVM 18.1.3 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR = ${LLVM_DIR}")

# 你的测试子目录（保持不变）
add_subdirectory(test)

# ---- 源码与库 ----
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(mylib ${LIB_SOURCES})

# 普通公共头
target_include_directories(mylib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# LLVM 头标为 system（生成 -isystem）
target_include_directories(mylib
  SYSTEM PUBLIC
    ${LLVM_INCLUDE_DIRS}
)

# 一些 LLVM 需要的编译开关（如 -fno-exceptions 等，按安装包而定）
target_compile_definitions(mylib PUBLIC ${LLVM_DEFINITIONS})

# 选择你需要的 LLVM 组件；IRBuilder 在 core 里，通常还需要 support/irreader 等
# 你后续若用到 Passes/BitWriter/ExecutionEngine/OrcJIT，可在此追加
llvm_map_components_to_libnames(LLVM_LIBS
  core
  support
  irreader
)

target_link_libraries(mylib PUBLIC ${LLVM_LIBS})

# ---- 可执行文件 ----
add_executable(code main.cpp)
target_link_libraries(code PRIVATE mylib)

# ---- 调试用 Sanitizers（保持不变）----
add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
add_link_options(-fsanitize=address,undefined)
