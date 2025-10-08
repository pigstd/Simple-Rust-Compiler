cmake_minimum_required(VERSION 3.22.1)
project(SimpleRustCompiler LANGUAGES CXX)   # 只启用 C++
project(rust-compiler)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(test)


file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(mylib ${LIB_SOURCES})
target_include_directories(mylib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

add_executable(code
    main.cpp
)
target_link_libraries(code PRIVATE mylib)


# 调试时可以打开，用来测越界的问题
target_compile_options(code PRIVATE -fsanitize=address,leak,undefined)
target_link_libraries(code PRIVATE -fsanitize=address,leak,undefined)