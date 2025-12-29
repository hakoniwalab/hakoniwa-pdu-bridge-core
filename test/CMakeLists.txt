find_package(GTest REQUIRED)

set(TEST_SOURCES
    bridge_loader_test.cpp
    bridge_core_flow_test.cpp
)

add_executable(hakoniwa_pdu_bridge_tests ${TEST_SOURCES})

target_include_directories(hakoniwa_pdu_bridge_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hakoniwa_pdu_bridge_tests
    PRIVATE
        TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
)

target_link_libraries(hakoniwa_pdu_bridge_tests
    PRIVATE
        hakoniwa_pdu_bridge_lib
        GTest::gtest_main
)

add_test(NAME hakoniwa_pdu_bridge_tests COMMAND hakoniwa_pdu_bridge_tests)
