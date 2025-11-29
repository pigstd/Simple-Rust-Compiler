# test/CMakeLists.txt
# 功能：
# 1) 自动发现 test/ 下的一级子目录（每个目录 = 某库的测试）
# 2) 为该目录中的每个 *.cpp 生成一个测试可执行文件
# 3) 可执行文件输出到 <源码根>/test/build/<库名>/

# 统一设置测试可执行默认输出根目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/test/build")

# 列出 test/ 下的所有一级子目录
file(GLOB TEST_SUBDIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*")

foreach(sub ${TEST_SUBDIRS})
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${sub}")

    # 找到该库测试目录下所有 *.cpp
    file(GLOB TEST_SRCS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${sub}"
         "${CMAKE_CURRENT_SOURCE_DIR}/${sub}/*.cpp")

    foreach(src ${TEST_SRCS})
      get_filename_component(name_we "${src}" NAME_WE)
      set(tgt "${name_we}")
      if("${sub}" STREQUAL "IRGen_with_runtime" AND "${name_we}" STREQUAL "ir_program_driver")
        set(tgt "ir_program_driver_with_runtime")
      endif()
      set(src_abs "${CMAKE_CURRENT_SOURCE_DIR}/${sub}/${src}")

      add_executable(${tgt} "${src_abs}")

      # 让测试能 #include 外部公共头文件（如 include/foo.h）
      target_include_directories(${tgt} PRIVATE "${CMAKE_SOURCE_DIR}/include")

      # 链接规则：
      # 链接 mylib
      target_link_libraries(${tgt} PRIVATE mylib)

      # 该测试可执行放到 test/build/<库名>/ 下
      set_target_properties(${tgt} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/test/build/${sub}"
      )

      target_compile_options(${tgt} PRIVATE -Wall)
    endforeach()
  endif()
endforeach()
