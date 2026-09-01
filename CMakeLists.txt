cmake_minimum_required(VERSION 3.21)
project(StatusBar_for_Coder)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h")
set(RC_FILES "src/platform/app_resource.rc")

add_executable(${PROJECT_NAME} ${SRC_FILES} ${RC_FILES} resources.qrc)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Qml 
    Qt6::Quick
    Shell32 
    User32 
    Gdi32
    Powrprof
    Pdh
)
