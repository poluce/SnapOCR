cmake_minimum_required(VERSION 3.21)

project(SnapOCR
    VERSION 1.0.0
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

# 源文件自动收集：新增 .cpp 无需改 CMakeLists（Ninja 会在下一次构建时重新配置）。
# 约定：src/ 下除 main.cpp 外全部属于 snapocr_core；新增模块直接放对应子目录即可。
file(GLOB_RECURSE SNAPCORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
list(REMOVE_ITEM SNAPCORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(snapocr_core STATIC ${SNAPCORE_SOURCES})

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

if(WIN32)
    # C++/WinRT（Windows.Media.Ocr）与系统 API
    # windowsapp：C++/WinRT（Windows.Media.Ocr）；dwmapi：窗口可见边框/Cloaked 判定
    target_link_libraries(snapocr_core PUBLIC windowsapp dwmapi)
endif()

snapocr_set_common_options(snapocr_core)

# ---- 界面翻译（阶段三 i18n 骨架）----
# .ts 源文件在 translations/；用 Qt 自带的 lrelease 编译成 .qm，
# 输出到 build/translations 并复制到 exe 同目录（运行期从那里加载）。
find_program(SNAPCOR_LRELEASE
    NAMES lrelease lrelease.exe
    HINTS "${QT_BIN_DIR}" "$ENV{QTDIR}/bin"
    PATH_SUFFIXES ""
)
set(SNAPCOR_TRANSLATIONS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
if(SNAPCOR_LRELEASE)
    set(SNAPCOR_QM_FILES "")
    file(GLOB SNAPCOR_TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")
    foreach(ts_file ${SNAPCOR_TS_FILES})
        get_filename_component(ts_name ${ts_file} NAME_WE)
        set(qm_file "${SNAPCOR_TRANSLATIONS_OUTPUT_DIR}/${ts_name}.qm")
        add_custom_command(
            OUTPUT ${qm_file}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SNAPCOR_TRANSLATIONS_OUTPUT_DIR}
            COMMAND ${SNAPCOR_LRELEASE} ${ts_file} -qm ${qm_file}
            DEPENDS ${ts_file}
            COMMENT "lrelease ${ts_name}.ts"
            VERBATIM
        )
        list(APPEND SNAPCOR_QM_FILES ${qm_file})
    endforeach()
    if(SNAPCOR_QM_FILES)
        add_custom_target(snapocr_translations ALL DEPENDS ${SNAPCOR_QM_FILES})
    endif()
else()
    message(STATUS "lrelease not found - UI translations will not be compiled")
endif()

# 产品可执行文件：Windows GUI 子系统，运行时不弹控制台
add_executable(SnapOCR WIN32
    src/main.cpp
)

if(TARGET snapocr_translations)
    add_dependencies(SnapOCR snapocr_translations)
    add_custom_command(TARGET SnapOCR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SNAPCOR_TRANSLATIONS_OUTPUT_DIR}"
                "$<TARGET_FILE_DIR:SnapOCR>/translations"
        COMMENT "Copy UI translations next to SnapOCR.exe"
    )
endif()

target_link_libraries(SnapOCR PRIVATE snapocr_core)
snapocr_set_common_options(SnapOCR)

enable_testing()
add_subdirectory(tests)
