# 统一 VS 和命令行的输出布局；缓存按项目入口分开，避免混用源码根目录。
include_guard(GLOBAL)

get_filename_component(seisSimWorkspaceRoot "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
set(seisSimOutputRoot "${seisSimWorkspaceRoot}/out")

# CMake 的缓存位置必须在配置命令或 VS 设置中指定，不能在 project() 后重定向。
# 在加载编译器之前拒绝其他位置，防止误把中间文件写入源码目录。
file(RELATIVE_PATH seisSimBuildRelativePath "${seisSimOutputRoot}" "${CMAKE_BINARY_DIR}")
if(IS_ABSOLUTE "${seisSimBuildRelativePath}"
   OR seisSimBuildRelativePath STREQUAL ".."
   OR seisSimBuildRelativePath MATCHES "^\\.\\./")
    message(FATAL_ERROR
        "Build files must be placed inside ${seisSimOutputRoot}.\n"
        "Open the workspace root in Visual Studio, or use:\n"
        "  cmake -S \"${seisSimWorkspaceRoot}\" -B \"${seisSimOutputRoot}/build/x64-Debug\" -G Ninja -DCMAKE_BUILD_TYPE=Debug\n"
        "The inner project's CMakeSettings.json also selects an external build directory.")
endif()

# 使用配置表达式，兼容 Ninja 和 Visual Studio 多配置生成器，避免重复配置子目录。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${seisSimOutputRoot}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${seisSimOutputRoot}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${seisSimOutputRoot}/lib/$<CONFIG>")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${seisSimOutputRoot}/bin/$<CONFIG>")

# 各入口的编译 PDB 留在各自的构建目录，避免并行编译时相互覆盖。
set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb/$<CONFIG>")
