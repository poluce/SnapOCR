# 每个测试文件独立成可执行文件（QTEST_GUILESS_MAIN 会定义 main，不能多个文件链到一起）。
# 约定：新增 tests/test_*.cpp 自动注册为测试目标，无需改本文件。
function(snapocr_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE snapocr_core Qt6::Test)
    snapocr_set_common_options(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # i18n 测试需要和运行期一致的翻译目录
    if(TARGET snapocr_translations)
        add_dependencies(${name} snapocr_translations)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${SNAPCOR_TRANSLATIONS_OUTPUT_DIR}"
                    "$<TARGET_FILE_DIR:${name}>/translations")
    endif()
endfunction()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(test_source IN LISTS TEST_SOURCES)
    get_filename_component(test_name "${test_source}" NAME_WE)
    snapocr_add_test(${test_name} "${test_source}")
endforeach()

# 冒烟测试：真实启动一次应用（offscreen 平台，CI 无桌面也能跑）
add_test(NAME snapocr_smoke COMMAND SnapOCR --smoke-test)
set_tests_properties(snapocr_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
