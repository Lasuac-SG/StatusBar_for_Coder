cmake_minimum_required(VERSION 3.21)
project(StatusBar_for_Coder)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启 Qt 自动化编译工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

# 收集所有 C++ 源文件与头文件
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h")
set(RC_FILES "src/platform/app_resource.rc")

# 1. 声明可执行文件目标 (必须放在 target_* 指令的最前面)
add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES} ${RC_FILES} resources.qrc)

# 2. 配置头文件包含目录
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party  # 让编译器能找到 nlohmann/json.hpp
)

# 3. 链接所需的动态/静态库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    # Qt6 组件
    Qt6::Core 
    Qt6::Gui 
    Qt6::Qml 
    Qt6::Quick
    # Windows 底层 API 核心库 (系统托盘、AppBar 所需)
    Shell32 
    User32 
    Gdi32
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
