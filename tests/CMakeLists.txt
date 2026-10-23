add_executable(snapocr_tests
    test_version.cpp
)

target_link_libraries(snapocr_tests PRIVATE
    snapocr_core
    Qt6::Test
)

snapocr_set_common_options(snapocr_tests)

add_test(NAME snapocr_tests COMMAND snapocr_tests)
add_test(NAME snapocr_smoke COMMAND SnapOCR --smoke-test)

# 单元测试使用 QTEST_GUILESS_MAIN；冒烟测试在 CI/无桌面环境用 offscreen 平台
set_tests_properties(snapocr_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(snapocr_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
