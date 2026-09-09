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
