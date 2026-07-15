cmake_minimum_required(VERSION 3.21)

project(codex-wxpush VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(CODEX_WXPUSH_SOURCES
    src/main.cpp
    src/AppSettings.cpp
    src/AppSettings.h
    src/HookHandler.cpp
    src/HookHandler.h
    src/HookInstaller.cpp
    src/HookInstaller.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/TrayApp.cpp
    src/TrayApp.h
    src/WxPushClient.cpp
    src/WxPushClient.h
)

qt_add_executable(codex-wxpush
    ${CODEX_WXPUSH_SOURCES}
)

qt_add_executable(codex-wxpush-hook
    ${CODEX_WXPUSH_SOURCES}
)

target_link_libraries(codex-wxpush PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_link_libraries(codex-wxpush-hook PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    target_compile_definitions(codex-wxpush PRIVATE NOMINMAX)
    target_compile_definitions(codex-wxpush-hook PRIVATE NOMINMAX)
    set_target_properties(codex-wxpush PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
