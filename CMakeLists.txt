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
    src/core/config_repository.h
    src/core/config_repository.cpp
    src/core/layout_engine.h
    src/core/layout_engine.cpp
    src/platform/appbar_proxy.h
    src/platform/appbar_proxy.cpp
    src/platform/cpu_service.h
    src/platform/cpu_service.cpp
    src/platform/display.h
    src/platform/display.cpp
    src/platform/tray_icon.h
    src/platform/tray_icon.cpp
    src/ui/widget_model.h
    src/ui/widget_model.cpp
    src/widgets/widget_view_model.h
    src/widgets/clock/clock_view_model.h
    src/widgets/clock/clock_view_model.cpp
    src/widgets/cpu/cpu_view_model.h
    src/widgets/cpu/cpu_view_model.cpp
    src/widgets/registry_setup.h
    src/widgets/registry_setup.cpp
)
target_link_libraries(statusbar_core
    PUBLIC Qt6::Core
    PRIVATE Shell32 User32 Pdh Advapi32
)
target_include_directories(statusbar_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
qt_extract_metatypes(statusbar_core)

qt_add_executable(StatusBar_for_Coder WIN32)
target_sources(StatusBar_for_Coder PRIVATE
    src/main.cpp
    src/platform/app_resource.rc
)

set_source_files_properties(src/ui/qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS Theme.qml
)
set_source_files_properties(src/ui/qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(src/ui/qml/WidgetHost.qml PROPERTIES QT_RESOURCE_ALIAS WidgetHost.qml)
set_source_files_properties(src/ui/qml/MetricCard.qml PROPERTIES QT_RESOURCE_ALIAS MetricCard.qml)
set_source_files_properties(src/widgets/clock/ClockWidget.qml PROPERTIES
    QT_RESOURCE_ALIAS ClockWidget.qml)
set_source_files_properties(src/widgets/cpu/CpuWidget.qml PROPERTIES
    QT_RESOURCE_ALIAS CpuWidget.qml)
set_source_files_properties(src/widgets/cpu/CpuDetailPopup.qml PROPERTIES
    QT_RESOURCE_ALIAS CpuDetailPopup.qml)
set_source_files_properties(src/widgets/cpu/cpu.svg PROPERTIES QT_RESOURCE_ALIAS cpu.svg)

qt_add_library(StatusBarQml STATIC)
qt_add_qml_module(StatusBarQml
    URI StatusBar
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/StatusBar"
    SOURCES
        src/ui/qml_types.h
        src/ui/window_geometry.h
        src/ui/window_geometry.cpp
    QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/Theme.qml
        src/ui/qml/WidgetHost.qml
        src/ui/qml/MetricCard.qml
        src/widgets/clock/ClockWidget.qml
        src/widgets/cpu/CpuWidget.qml
        src/widgets/cpu/CpuDetailPopup.qml
    RESOURCES
        src/widgets/cpu/cpu.svg
)
target_include_directories(StatusBarQml PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)
target_link_libraries(StatusBarQml PUBLIC
    statusbar_core Qt6::Gui Qt6::Qml Qt6::Quick)

add_library(statusbar_application STATIC)
target_sources(statusbar_application PRIVATE
    src/ui/qt_application.h
    src/ui/qt_application.cpp
)
target_include_directories(statusbar_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(statusbar_application PUBLIC
    statusbar_core Qt6::Gui Qt6::Qml Qt6::Quick)

target_link_libraries(StatusBar_for_Coder PRIVATE
    statusbar_application StatusBarQml StatusBarQmlplugin
    Qt6::Gui Qt6::Qml Qt6::Quick)

if(MSVC)
    target_compile_options(statusbar_core PRIVATE /W4 /permissive-)
    target_compile_options(StatusBarQml PRIVATE /W4 /permissive-)
    target_compile_options(statusbar_application PRIVATE /W4 /permissive-)
    target_compile_options(StatusBar_for_Coder PRIVATE /W4 /permissive-)
else()
    target_compile_options(statusbar_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(StatusBarQml PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(statusbar_application PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(StatusBar_for_Coder PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
