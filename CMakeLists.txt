cmake_minimum_required(VERSION 3.21)

project(SnapOCR
    VERSION 0.1.0
    DESCRIPTION "Windows 截图、标注与 OCR 工具"
    HOMEPAGE_URL "https://github.com/poluce/SnapOCR"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Test)

# 版本信息由 CMake 统一注入，避免源码与 project(VERSION) 漂移
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/snapocr/Version.h"
    @ONLY
)

# 统一编译选项：UTF-8、严格警告、Windows 宽字符
function(snapocr_set_common_options target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /utf-8
            /W4
            /permissive-
            /Zc:__cplusplus
            /EHsc
        )
        target_compile_definitions(${target} PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -finput-charset=UTF-8
            -fexec-charset=UTF-8
        )
    endif()
endfunction()

# 业务核心静态库：UI 壳、可复用逻辑、平台封装均放这里，便于单测直接链接
add_library(snapocr_core STATIC
    src/app/Application.cpp
    src/app/Application.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/platform/win32/Console.cpp
    src/platform/win32/Console.h
)

target_include_directories(snapocr_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(snapocr_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Concurrent
)

snapocr_set_common_options(snapocr_core)

# 产品可执行文件：Windows GUI 子系统，运行时不弹控制台
add_executable(SnapOCR WIN32
    src/main.cpp
)

target_link_libraries(SnapOCR PRIVATE snapocr_core)
snapocr_set_common_options(SnapOCR)

enable_testing()
add_subdirectory(tests)
