cmake_minimum_required(VERSION 3.16)
project(hakoniwa-pdu-bridge CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(include)
# This allows includes like #include "hakoniwa/pdu/endpoint.hpp"
include_directories(hakoniwa-pdu-endpoint/include)

# The submodule 'hakoniwa-pdu-endpoint' will fetch nlohmann_json.
# We add the subdirectory first, making its targets (including fetched ones) available.
add_subdirectory(hakoniwa-pdu-endpoint)

# Find all our source files
file(GLOB_RECURSE PDU_BRIDGE_SOURCES "src/*.cpp")
set(PDU_BRIDGE_LIB_SOURCES ${PDU_BRIDGE_SOURCES})
list(REMOVE_ITEM PDU_BRIDGE_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/daemon.cpp")

# Create a library from the source files (excluding main)
add_library(hakoniwa_pdu_bridge_lib ${PDU_BRIDGE_LIB_SOURCES})

# Add the executable
add_executable(hakoniwa-pdu-bridge src/daemon.cpp)

# Explicitly add the hakoniwa core include directory for this project
if(APPLE)
  target_include_directories(hakoniwa_pdu_bridge_lib
    PRIVATE
      /usr/local/hakoniwa/include/hakoniwa
  )
endif()

target_link_libraries(hakoniwa_pdu_bridge_lib PUBLIC hakoniwa_pdu_endpoint nlohmann_json::nlohmann_json)

# Link our bridge executable against the created library
target_link_libraries(hakoniwa-pdu-bridge PRIVATE hakoniwa_pdu_bridge_lib)

# Enable testing
enable_testing()

# Add our test subdirectory
#add_subdirectory(test)

