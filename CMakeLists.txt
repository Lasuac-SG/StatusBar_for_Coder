cmake_minimum_required(VERSION 3.21)
project(StatusBar_for_Coder VERSION 0.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

add_library(statusbar_core STATIC)
target_sources(statusbar_core PRIVATE
    src/core/config_manager.cpp
    src/core/window_manager.cpp
    src/platform/appbar_proxy.cpp
    src/platform/display.cpp
    src/platform/tray_icon.cpp
    src/ui/widget_model.cpp
    src/widgets/clock/clock_adapter.h
    src/widgets/clock/clock_view_model.cpp
    src/widgets/cpu/cpu_adapter.cpp
    src/widgets/registry_setup.cpp
)
target_link_libraries(statusbar_core
    PUBLIC Qt6::Core
    PRIVATE Shell32 User32 Pdh Advapi32
)
target_include_directories(statusbar_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(statusbar_core SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

qt_add_executable(StatusBar_for_Coder WIN32)
target_sources(StatusBar_for_Coder PRIVATE
    src/main.cpp
    src/platform/app_resource.rc
    src/ui/qt_window_adapter.cpp
    resources.qrc
)
target_link_libraries(StatusBar_for_Coder PRIVATE
    statusbar_core Qt6::Gui Qt6::Qml Qt6::Quick)

if(MSVC)
    target_compile_options(statusbar_core PRIVATE /W4 /permissive-)
    target_compile_options(StatusBar_for_Coder PRIVATE /W4 /permissive-)
else()
    target_compile_options(statusbar_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(StatusBar_for_Coder PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
