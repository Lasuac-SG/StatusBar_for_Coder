cmake_minimum_required(VERSION 3.21)
project(StatusBar_for_Coder VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 强制使用 C++20，保证现代 C++ 特性的可用性
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. 自动下载并引入 Slint (沿用 release/1 标签)
include(FetchContent)
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG release/1 
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)


set(SRC_FILES
    src/main.cpp
    src/platform/display.cpp
    src/platform/appbar_proxy.cpp
    src/core/window_manager.cpp
    ui/slint_window_adapter.cpp
)

# 3. 定义可执行文件
add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})

# 4. 引入 Slint UI 文件
slint_target_sources(${PROJECT_NAME} ui/main.slint)

# 5. 链接库依赖：这里新增了 Windows 的 Shell32 (AppBar 需要) 和 User32 (FindWindow 需要)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Slint::Slint 
    Shell32 
    User32
    gdi32
    comctl32
)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 6. Windows DLL 拷贝 (保持你在上一步加的代码不变)
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()
