find_package(GTest REQUIRED)

set(TEST_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config" CACHE PATH "Test config root directory")

set(TEST_SOURCES
    bridge_loader_test.cpp
    bridge_core_flow_test.cpp
    bridge_tcp_flow_test.cpp
    bridge_connection_test.cpp
    ondemand_control_handler_test.cpp
    monitor_cli_utils_test.cpp
)

add_executable(hakoniwa_pdu_bridge_tests ${TEST_SOURCES})

target_include_directories(hakoniwa_pdu_bridge_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(hakoniwa_pdu_bridge_tests
    PRIVATE
        TEST_CONFIG_DIR="${TEST_CONFIG_DIR}"
)

target_link_libraries(hakoniwa_pdu_bridge_tests
    PRIVATE
        hakoniwa_pdu_bridge_lib
        GTest::gtest_main
)

# Keep the historical Core-enabled link path for direct CMake users, while
# allowing the manifest-driven standalone build to remain Core-independent.
if(HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
    target_link_libraries(hakoniwa_pdu_bridge_tests PRIVATE ${HAKO_PDU_BRIDGE_CORE_LIBS})
    target_link_directories(hakoniwa_pdu_bridge_tests
        PRIVATE
            "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib"
    )
endif()

add_test(NAME hakoniwa_pdu_bridge_tests COMMAND hakoniwa_pdu_bridge_tests)
