if(NOT TARGET statusbar_core)
    message(FATAL_ERROR "statusbar_core target is required")
endif()
qt_add_executable(statusbar_smoke_test smoke_test.cpp)
target_link_libraries(statusbar_smoke_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME statusbar_smoke_test COMMAND statusbar_smoke_test)

qt_add_executable(layout_engine_test layout_engine_test.cpp)
target_link_libraries(layout_engine_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME layout_engine_test COMMAND layout_engine_test)

qt_add_executable(config_repository_test config_repository_test.cpp)
target_link_libraries(config_repository_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME config_repository_test COMMAND config_repository_test)

qt_add_executable(cpu_usage_test cpu_usage_test.cpp)
target_link_libraries(cpu_usage_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(cpu_usage_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_test(NAME cpu_usage_test COMMAND cpu_usage_test)

qt_add_executable(widget_instances_test widget_instances_test.cpp)
target_link_libraries(widget_instances_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME widget_instances_test COMMAND widget_instances_test)

qt_add_executable(widget_registry_test widget_registry_test.cpp)
target_link_libraries(widget_registry_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME widget_registry_test COMMAND widget_registry_test)

qt_add_executable(widget_model_test widget_model_test.cpp)
target_link_libraries(widget_model_test PRIVATE statusbar_core Qt6::Test)
add_test(NAME widget_model_test COMMAND widget_model_test)

qt_add_executable(resource_embedding_test resource_embedding_test.cpp)
target_link_libraries(resource_embedding_test PRIVATE
    statusbar_core StatusBarQml StatusBarQmlplugin
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
add_test(NAME resource_embedding_test COMMAND resource_embedding_test)
set_tests_properties(resource_embedding_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

set_source_files_properties(qml/WarningRoot.qml PROPERTIES
    QT_RESOURCE_ALIAS WarningRoot.qml)
set_source_files_properties(qml/InitialWarningWidget.qml PROPERTIES
    QT_RESOURCE_ALIAS InitialWarningWidget.qml)
qt_add_library(QtApplicationWarningQml STATIC)
qt_add_qml_module(QtApplicationWarningQml
    URI StatusBar.TestWarnings
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/StatusBar/TestWarnings
    DEPENDENCIES TARGET StatusBarQml
    QML_FILES
        qml/WarningRoot.qml
        qml/InitialWarningWidget.qml)

qt_add_executable(qt_application_test qt_application_test.cpp)
target_link_libraries(qt_application_test PRIVATE
    statusbar_application StatusBarQml StatusBarQmlplugin
    QtApplicationWarningQml QtApplicationWarningQmlplugin
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
foreach(scenario IN ITEMS
        success
        config_failure
        missing_root
        warning_only
        model_cleanup
        run_lifecycle
        external_root_destruction
        initial_widget_failure)
    add_test(
        NAME qt_application_${scenario}
        COMMAND qt_application_test ${scenario})
    set_tests_properties(qt_application_${scenario} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
