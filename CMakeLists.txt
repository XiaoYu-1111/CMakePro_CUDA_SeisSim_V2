# ==========================================
# 1. 基础环境配置 (必须在 project 之前)
# ==========================================
cmake_minimum_required(VERSION 3.18) # 建议最低 3.18 以支持现代 CUDA 属性

# 直接在 VS 中打开本目录时，也使用外层 out 存放编译产物。
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/BuildLayout.cmake")

# [核心修复] 针对 MSVC + Ninja 环境下的 NVCC 路径转义 Bug
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded) # 强制 /Z7 嵌入调试信息
set(CMAKE_CUDA_COMPILE_OPTIONS_MSVC_PDB_OUTPUT_PATH "" CACHE STRING "" FORCE) # 彻底禁用 -Fd 参数

# 设置显卡架构 (RTX 4060 对应 89)
set(CMAKE_CUDA_ARCHITECTURES native)

# [关键修改] 显式声明支持 CUDA 语言
project(CMakePro_CUDA_SeisSim_V1 LANGUAGES CXX C CUDA)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/MsvcDependencies.cmake")

set(TARGET_NAME "CMakePro_CUDA_SeisSim_V2")

# ==========================================
# 2. 第三方库路径设置 (保留绝对路径方式)
# ==========================================
set(LIB_ROOT "D:/CPP_lib")
set(OPENGL_ROOT "${LIB_ROOT}/OpenGL")
set(SDL2_ROOT   "${LIB_ROOT}/SDL2/SDL2-devel-2.30.9-VC/SDL2-2.30.9")
set(CUDAToolkit_ROOT "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8")

# ==========================================
# 3. 收集源文件
# ==========================================
# ImGui & ImPlot (Docking 分支)
file(GLOB IMGUIDocking_SRCS
    "ImGui_docking/*.cpp"
    "ImPlot/*.cpp"
)

# CUDA 核函数文件
file(GLOB CUDA_USER_SOURCES "CUDA_Kernels/Cuda_Check.cu")
file(GLOB CUDA_USER_HEADERS "CUDA_Kernels/Cuda_Check.cuh")

# 项目主源文件
set(PROJECT_SRCS "Main_cpp/CMakePro_cuda.cpp")

# STB Image (仅头文件)
set(STB_SRCS "stb_image/stb_image_write.h")

# ==========================================
# 4. 创建可执行文件
# ==========================================
add_executable(${TARGET_NAME}
    ${IMGUIDocking_SRCS}
    ${STB_SRCS}
    ${PROJECT_SRCS} 
    ${CUDA_USER_SOURCES}
    ${CUDA_USER_HEADERS}
    "Main_header/Common.h"
    "Main_header/ResourcePaths.h"
    "Main_cpp/ResourcePaths.cpp"
    "Main_cpp/SeismicIO.cpp")

# ==========================================
# 5. 编译选项与标准设置
# ==========================================
set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CUDA_STANDARD 17
    CUDA_RUNTIME_LIBRARY Shared
    MSVC_DEBUG_INFORMATION_FORMAT Embedded
)

#解决cuda链接出错问题
if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/FS>
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
    )
endif()

# 针对不同语言的特定编译选项
target_compile_options(${TARGET_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:
        --use_fast_math
        -Xcompiler=/wd4819
        -Xcompiler=/utf-8
        -Xcompiler=/Z7
    >
)

# ==========================================
# 6. 资源文件同步 (resource_CUDA_V1)
# ==========================================
set(RESOURCE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resource_CUDA_V1")
set(RESOURCE_DEST_DIR "$<TARGET_FILE_DIR:${TARGET_NAME}>/resource_CUDA_V1")

file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS "${RESOURCE_SRC_DIR}/*")

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${RESOURCE_SRC_DIR}" "${RESOURCE_DEST_DIR}"
    COMMENT "Syncing resource files..."
    VERBATIM
)
# 修改字体或着色器时也重新执行链接后的部署步骤。
set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_DEPENDS ${RESOURCE_FILES})

# ==========================================
# 7. 配置头文件路径 (Include)
# ==========================================
target_include_directories(${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Main_header"
    "${CMAKE_CURRENT_SOURCE_DIR}/ImGui_docking"
    "${CMAKE_CURRENT_SOURCE_DIR}/ImPlot"
    "${CMAKE_CURRENT_SOURCE_DIR}/stb_image"
    "${CMAKE_CURRENT_SOURCE_DIR}/CUDA_Kernels"
    
    # 第三方库
    "${OPENGL_ROOT}/glm-master/glm-master"
    "${OPENGL_ROOT}/glew-2.1.0-win32/glew-2.1.0/include"
    "${OPENGL_ROOT}/glfw-3.4.bin.WIN64/glfw-3.4.bin.WIN64/include"
    "${SDL2_ROOT}/include"
    "${CUDAToolkit_ROOT}/include"
)

# ==========================================
# 8. 配置库路径 (Link Directories)
# ==========================================
target_link_directories(${TARGET_NAME} PRIVATE
    "${OPENGL_ROOT}/glfw-3.4.bin.WIN64/glfw-3.4.bin.WIN64/lib-vc2022"
    "${OPENGL_ROOT}/glew-2.1.0-win32/glew-2.1.0/lib/Release/x64"
    "${SDL2_ROOT}/lib/x64"
    "${CUDAToolkit_ROOT}/lib/x64"
)

# ==========================================
# 9. 链接库与定义
# ==========================================
target_compile_definitions(${TARGET_NAME} PRIVATE GLEW_STATIC)

target_link_libraries(${TARGET_NAME} PRIVATE
    # OpenGL
    glfw3.lib
    glew32s.lib
    opengl32.lib
    user32.lib
    gdi32.lib
    shell32.lib
    
    # SDL2
    #SDL2.lib
    #SDL2_image.lib
    #SDL2_mixer.lib

    # CUDA (显式路径下的库名)
    cudart.lib
    cuda.lib
)

# ==========================================
# 10. IDE 视图美化 (可选)
# ==========================================
source_group("Source Files\\CUDA" FILES ${CUDA_USER_SOURCES} ${CUDA_USER_HEADERS})
source_group("Source Files\\ImGui" FILES ${IMGUIDocking_SRCS})

# 三维求解与渲染拥有独立编译目标，不复用二维上下文或快速数学编译选项。
add_library(Seis3D STATIC
    "include/Common3D.h" "include/Seis3D_Sim.h"
    "cuda/Cuda_Seis3D.cuh" "cuda/Cuda_Seis3D.cu" "src/Seis3D_Sim.cpp"
    "src/Seis3D/Renderer3D.cpp" "src/Seis3D/Workspace3D.cpp" "src/Seis3D/SliceView3D.cpp" "src/Seis3D/TraceView3D.cpp"
    "src/Seis3D/ModelIo3D.cpp" "src/Seis3D/Acquisition3D.cpp"
    "src/Seis3D/Internal3D.h" "src/Seis3D/Visual3D.h")
get_target_property(SEIS3D_SHARED_INCLUDE_DIRS ${TARGET_NAME} INCLUDE_DIRECTORIES)
get_target_property(SEIS3D_SHARED_LINK_DIRS ${TARGET_NAME} LINK_DIRECTORIES)
add_library(AppUi STATIC "Main_header/AppTheme.h" "Main_cpp/AppTheme.cpp"
    "Main_header/HomeScreen.h" "Main_cpp/HomeScreen.cpp")
target_include_directories(AppUi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Main_header" PRIVATE ${SEIS3D_SHARED_INCLUDE_DIRS})
target_compile_definitions(AppUi PRIVATE GLEW_STATIC)
set_target_properties(AppUi PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON MSVC_DEBUG_INFORMATION_FORMAT Embedded)
if(MSVC)
    target_compile_options(AppUi PRIVATE /utf-8)
endif()
target_include_directories(Seis3D PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/cuda")
target_include_directories(Seis3D PRIVATE ${SEIS3D_SHARED_INCLUDE_DIRS})
target_link_directories(Seis3D PRIVATE ${SEIS3D_SHARED_LINK_DIRS})
target_compile_definitions(Seis3D PRIVATE GLEW_STATIC)
set_target_properties(Seis3D PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
    CUDA_STANDARD 17 CUDA_RUNTIME_LIBRARY Shared MSVC_DEBUG_INFORMATION_FORMAT Embedded)
if(MSVC)
    target_compile_options(Seis3D PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/utf-8>)
endif()
target_link_libraries(Seis3D PRIVATE AppUi cudart.lib glew32s.lib opengl32.lib)
target_link_libraries(${TARGET_NAME} PRIVATE AppUi Seis3D)
source_group("Seis3D\\Workspace" FILES "src/Seis3D/Workspace3D.cpp" "src/Seis3D/SliceView3D.cpp" "src/Seis3D/TraceView3D.cpp" "src/Seis3D/Visual3D.h")
source_group("Seis3D\\Rendering" FILES "src/Seis3D/Renderer3D.cpp")
source_group("Seis3D\\Data IO" FILES "src/Seis3D/ModelIo3D.cpp" "src/Seis3D/Acquisition3D.cpp")
file(GLOB SEIS3D_SHADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*")
set_property(TARGET ${TARGET_NAME} APPEND PROPERTY LINK_DEPENDS ${SEIS3D_SHADER_FILES})
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/resource_Seis3D"
    COMMENT "Deploying independent Seis3D shaders..." VERBATIM)

# Optional focused graphics regression; runs without displaying an application window.
option(SEIS3D_BUILD_WORKSPACE_CHECKS "Build CUDA/GL workspace regression" OFF)
if(SEIS3D_BUILD_WORKSPACE_CHECKS)
    add_executable(Seis3DWorkspaceRegression "tests/Seis3DWorkspaceRegression.cpp"
        "Main_cpp/ResourcePaths.cpp" ${IMGUIDocking_SRCS})
    target_include_directories(Seis3DWorkspaceRegression PRIVATE ${SEIS3D_SHARED_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_directories(Seis3DWorkspaceRegression PRIVATE ${SEIS3D_SHARED_LINK_DIRS})
    target_compile_definitions(Seis3DWorkspaceRegression PRIVATE GLEW_STATIC)
    set_target_properties(Seis3DWorkspaceRegression PROPERTIES CXX_STANDARD 20 MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    if(MSVC)
        target_compile_options(Seis3DWorkspaceRegression PRIVATE /utf-8)
    endif()
    target_link_libraries(Seis3DWorkspaceRegression PRIVATE Seis3D glfw3.lib glew32s.lib opengl32.lib cudart.lib user32.lib gdi32.lib shell32.lib)
    add_dependencies(Seis3DWorkspaceRegression ${TARGET_NAME})
    add_test(NAME Seis3DWorkspaceRegression COMMAND Seis3DWorkspaceRegression)
    set_tests_properties(Seis3DWorkspaceRegression PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
endif()

# 无图形窗口的二维数值回归程序，直接调用生产 CUDA 求解器。
if(SEISSIM_BUILD_SOLVER_TESTS)
    add_executable(SeisSimSolverRegression "tests/SolverRegression.cpp" ${CUDA_USER_SOURCES})
    get_target_property(SOLVER_TEST_INCLUDES ${TARGET_NAME} INCLUDE_DIRECTORIES)
    get_target_property(SOLVER_TEST_LINK_DIRS ${TARGET_NAME} LINK_DIRECTORIES)
    get_target_property(SOLVER_TEST_OPTIONS ${TARGET_NAME} COMPILE_OPTIONS)
    target_include_directories(SeisSimSolverRegression PRIVATE ${SOLVER_TEST_INCLUDES})
    target_link_directories(SeisSimSolverRegression PRIVATE ${SOLVER_TEST_LINK_DIRS})
    target_compile_options(SeisSimSolverRegression PRIVATE ${SOLVER_TEST_OPTIONS})
    target_compile_definitions(SeisSimSolverRegression PRIVATE GLEW_STATIC)
    set_target_properties(SeisSimSolverRegression PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CUDA_STANDARD 17 MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    target_link_libraries(SeisSimSolverRegression PRIVATE cudart.lib)
    add_test(NAME SolverRegression COMMAND SeisSimSolverRegression)
    set_tests_properties(SolverRegression PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77)
endif()
