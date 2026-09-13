cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()
project(StatusBar_for_Coder VERSION 0.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)
include(CTest)
include(CheckIPOSupported)

function(statusbar_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
        )
    endif()
endfunction()

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()

add_library(statusbar_core STATIC)
target_sources(statusbar_core PRIVATE
    src/core/config_repository.h
    src/core/config_repository.cpp
    src/core/layout_engine.h
    src/core/layout_engine.cpp
    src/platform/appbar.h
    src/platform/appbar.cpp
    src/platform/cpu_service.h
    src/platform/cpu_service.cpp
    src/platform/single_instance.h
    src/platform/single_instance.cpp
    src/platform/tray_icon.h
    src/platform/tray_icon.cpp
    src/platform/windows_shell_integration.h
    src/platform/windows_shell_integration.cpp
    src/platform/windows_logging.h
    src/platform/windows_logging.cpp
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

if(MINGW)
    target_link_options(StatusBar_for_Coder PRIVATE
        $<$<CONFIG:Release>:-Wl,--no-insert-timestamp>)
elseif(MSVC)
    target_link_options(StatusBar_for_Coder PRIVATE
        $<$<CONFIG:Release>:/Brepro>)
endif()

foreach(target IN ITEMS
        statusbar_core
        statusbar_application
        StatusBarQml
        StatusBarQmlplugin
        StatusBar_for_Coder)
    statusbar_enable_warnings(${target})
endforeach()

check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
if(ipo_supported)
    if(MINGW)
        # Qt adds -Wa,-mbig-obj to the generated type-registration unit. GCC's
        # LTO wrapper cannot merge that COFF variant, while non-IPO builds may
        # still need it for the section-count limit.
        set_property(SOURCE
            "${CMAKE_CURRENT_BINARY_DIR}/statusbarqml_qmltyperegistrations.cpp"
            PROPERTY COMPILE_OPTIONS
                "$<$<NOT:$<CONFIG:Release>>:-Wa,-mbig-obj>"
        )
    endif()
    set(statusbar_ipo_targets
        statusbar_core
        statusbar_application
        StatusBarQml
        StatusBarQmlplugin
        StatusBar_for_Coder
    )
    set_property(TARGET
        ${statusbar_ipo_targets}
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
else()
    message(STATUS "Release IPO/LTO is unavailable: ${ipo_error}")
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS StatusBar_for_Coder RUNTIME DESTINATION .)
install(FILES config.json DESTINATION .)
install(CODE [[
    get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)
    set(QT_DEPLOY_BIN_DIR ".")
    set(QT_DEPLOY_LIBEXEC_DIR ".")
    set(QT_DEPLOY_LIB_DIR ".")
    set(QT_DEPLOY_PLUGINS_DIR "plugins")
    set(QT_DEPLOY_QML_DIR "qml")
]])
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    qt_generate_deploy_qml_app_script(
        TARGET StatusBar_for_Coder
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        NO_TRANSLATIONS
        DEPLOY_TOOL_OPTIONS --compiler-runtime
        EXCLUDE_PLUGIN_TYPES
            generic
            iconengines
            networkinformation
            qmltooling
            sqldrivers
            tls
        EXCLUDE_PLUGINS qgif qico qjpeg
    )
else()
    qt_generate_deploy_qml_app_script(
        TARGET StatusBar_for_Coder
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        NO_TRANSLATIONS
        DEPLOY_TOOL_OPTIONS
            --compiler-runtime
            --skip-plugin-types
            generic,iconengines,networkinformation,qmltooling,sqldrivers,tls
            --exclude-plugins
            qgif,qico,qjpeg
    )
endif()
install(SCRIPT ${deploy_script})

if(MINGW)
    get_target_property(statusbar_qt_core_location
        Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT statusbar_qt_core_location)
        get_target_property(statusbar_qt_core_location
            Qt6::Core IMPORTED_LOCATION)
    endif()
    cmake_path(GET statusbar_qt_core_location PARENT_PATH statusbar_qt_bin_dir)
    configure_file(
        cmake/DeployMinGWRuntime.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/DeployMinGWRuntime.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/DeployMinGWRuntime.cmake")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" statusbar_system_processor)
if(statusbar_system_processor MATCHES "^(aarch64|arm64|arm64ec)$")
    set(statusbar_package_arch "arm64")
elseif(statusbar_system_processor MATCHES "^arm")
    set(statusbar_package_arch "arm")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(statusbar_package_arch "x86_64")
else()
    set(statusbar_package_arch "x86")
endif()
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "StatusBarForCoder")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "${PROJECT_NAME}-${PROJECT_VERSION}-windows-${statusbar_package_arch}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_STRIP_FILES TRUE)
include(CPack)
