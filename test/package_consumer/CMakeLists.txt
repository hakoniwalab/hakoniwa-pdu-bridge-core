cmake_minimum_required(VERSION 3.16)
project(hakoniwa_pdu_bridge_package_consumer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(hakoniwa_pdu_bridge CONFIG REQUIRED)

if(NOT TARGET hakoniwa_pdu_bridge::bridge)
  message(FATAL_ERROR "New Bridge package target hakoniwa_pdu_bridge::bridge is missing")
endif()

if(NOT TARGET hakoniwa_pdu_bridge::hakoniwa_pdu_bridge_lib)
  message(FATAL_ERROR "Legacy Bridge package target alias is missing")
endif()

add_executable(bridge_package_consumer main.cpp)
target_link_libraries(bridge_package_consumer PRIVATE hakoniwa_pdu_bridge::bridge)

# This target intentionally mirrors the package name probed by
# hakoniwa-conductor-pro so compatibility is checked without changing it first.
add_executable(bridge_legacy_package_consumer main.cpp)
target_link_libraries(bridge_legacy_package_consumer PRIVATE
  hakoniwa_pdu_bridge::hakoniwa_pdu_bridge_lib
)
