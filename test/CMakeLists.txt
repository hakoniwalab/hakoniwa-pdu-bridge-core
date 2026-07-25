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

# PDU Registry is a test-only dependency. The production Bridge library remains
# dependent only on hakoniwa-pdu-endpoint. Keep the historical hakoniwa/pdu/...
# include spelling used by existing tests via a generated compatibility include
# tree while sourcing the actual converter headers from the Registry checkout.
if(HAKO_PDU_REGISTRY_ROOT)
    set(HAKO_PDU_REGISTRY_TYPES_DIR "${HAKO_PDU_REGISTRY_ROOT}/pdu/types")
    set(HAKO_PDU_REGISTRY_COMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/pdu-registry-compat")

    if(NOT EXISTS "${HAKO_PDU_REGISTRY_TYPES_DIR}/pdu_convertor.hpp")
        message(FATAL_ERROR "PDU Registry pdu_convertor.hpp not found under ${HAKO_PDU_REGISTRY_TYPES_DIR}")
    endif()
    if(NOT EXISTS "${HAKO_PDU_REGISTRY_TYPES_DIR}/geometry_msgs/pdu_cpptype_conv_Twist.hpp")
        message(FATAL_ERROR "PDU Registry Twist converter not found under ${HAKO_PDU_REGISTRY_TYPES_DIR}/geometry_msgs")
    endif()

    file(MAKE_DIRECTORY
        "${HAKO_PDU_REGISTRY_COMPAT_DIR}/hakoniwa/pdu"
        "${HAKO_PDU_REGISTRY_COMPAT_DIR}/hakoniwa/pdu/geometry_msgs"
    )
    configure_file(
        "${HAKO_PDU_REGISTRY_TYPES_DIR}/pdu_convertor.hpp"
        "${HAKO_PDU_REGISTRY_COMPAT_DIR}/hakoniwa/pdu/pdu_convertor.hpp"
        COPYONLY
    )
    configure_file(
        "${HAKO_PDU_REGISTRY_TYPES_DIR}/geometry_msgs/pdu_cpptype_conv_Twist.hpp"
        "${HAKO_PDU_REGISTRY_COMPAT_DIR}/hakoniwa/pdu/geometry_msgs/pdu_cpptype_conv_Twist.hpp"
        COPYONLY
    )
    target_include_directories(hakoniwa_pdu_bridge_tests PRIVATE
        "${HAKO_PDU_REGISTRY_COMPAT_DIR}"
        "${HAKO_PDU_REGISTRY_TYPES_DIR}"
    )
elseif(NOT HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
    message(FATAL_ERROR
        "Bridge tests require a hakoniwa-pdu-registry checkout when Hakoniwa Core is disabled. "
        "Set HAKO_PDU_REGISTRY_ROOT or the HAKO_PDU_REGISTRY_ROOT environment variable."
    )
endif()

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
