cmake_minimum_required(VERSION 3.16)
project(hakoniwa-pdu-bridge CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakePackageConfigHelpers)

# Bridge public headers
include_directories(include)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

set(HAKO_PDU_ENDPOINT_PREFIX "/usr/local/hakoniwa" CACHE PATH
    "Install prefix of hakoniwa-pdu-endpoint")
find_path(HAKO_PDU_ENDPOINT_INCLUDE_DIR
  NAMES hakoniwa/pdu/endpoint.hpp
  HINTS
    "${HAKO_PDU_ENDPOINT_PREFIX}/include"
    "/usr/local/include"
    "/usr/local/hakoniwa/include"
)
find_library(HAKO_PDU_ENDPOINT_LIBRARY
  NAMES hakoniwa_pdu_endpoint
  HINTS
    "${HAKO_PDU_ENDPOINT_PREFIX}/lib"
    "/usr/local/lib"
    "/usr/local/hakoniwa/lib"
)
find_library(HAKO_ASSETS_LIBRARY
  NAMES assets
  HINTS
    "${HAKO_PDU_ENDPOINT_PREFIX}/lib"
    "/usr/local/hakoniwa/lib"
    "/usr/local/lib"
)
find_library(HAKO_SHAKOC_LIBRARY
  NAMES shakoc
  HINTS
    "${HAKO_PDU_ENDPOINT_PREFIX}/lib"
    "/usr/local/hakoniwa/lib"
    "/usr/local/lib"
)

if(NOT HAKO_PDU_ENDPOINT_INCLUDE_DIR)
  message(FATAL_ERROR "hakoniwa-pdu-endpoint header not found. Set HAKO_PDU_ENDPOINT_INCLUDE_DIR.")
endif()
if(NOT HAKO_PDU_ENDPOINT_LIBRARY)
  message(FATAL_ERROR "hakoniwa-pdu-endpoint library not found. Set HAKO_PDU_ENDPOINT_LIBRARY.")
endif()

set(HAKO_ENDPOINT_EXTRA_LIBS "")
list(APPEND HAKO_ENDPOINT_EXTRA_LIBS nlohmann_json::nlohmann_json)
if(HAKO_ASSETS_LIBRARY)
  list(APPEND HAKO_ENDPOINT_EXTRA_LIBS "${HAKO_ASSETS_LIBRARY}")
else()
  list(APPEND HAKO_ENDPOINT_EXTRA_LIBS assets)
endif()
if(HAKO_SHAKOC_LIBRARY)
  list(APPEND HAKO_ENDPOINT_EXTRA_LIBS "${HAKO_SHAKOC_LIBRARY}")
else()
  list(APPEND HAKO_ENDPOINT_EXTRA_LIBS shakoc)
endif()

if(NOT TARGET hakoniwa_pdu_endpoint)
  add_library(hakoniwa_pdu_endpoint UNKNOWN IMPORTED)
  set_target_properties(hakoniwa_pdu_endpoint PROPERTIES
    IMPORTED_LOCATION "${HAKO_PDU_ENDPOINT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
      "${HAKO_PDU_ENDPOINT_INCLUDE_DIR};${HAKO_PDU_ENDPOINT_INCLUDE_DIR}/hakoniwa;${HAKO_PDU_ENDPOINT_INCLUDE_DIR}/hakoniwa/pdu"
    INTERFACE_LINK_LIBRARIES "${HAKO_ENDPOINT_EXTRA_LIBS}"
  )
endif()

# Find all our source files
file(GLOB_RECURSE PDU_BRIDGE_SOURCES "src/*.cpp")
set(PDU_BRIDGE_LIB_SOURCES ${PDU_BRIDGE_SOURCES})
list(REMOVE_ITEM PDU_BRIDGE_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/daemon.cpp")

# Create a library from the source files (excluding main)
add_library(hakoniwa_pdu_bridge_lib ${PDU_BRIDGE_LIB_SOURCES})
target_include_directories(hakoniwa_pdu_bridge_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add the executable
add_executable(hakoniwa-pdu-bridge src/daemon.cpp)

# Explicitly add the hakoniwa core include directory for this project
if(APPLE)
  target_include_directories(hakoniwa_pdu_bridge_lib
    PRIVATE
      /usr/local/hakoniwa/include/hakoniwa
  )
endif()

target_link_libraries(hakoniwa_pdu_bridge_lib PUBLIC hakoniwa_pdu_endpoint)

# Link our bridge executable against the created library
target_link_libraries(hakoniwa-pdu-bridge 
  PRIVATE 
  hakoniwa_pdu_bridge_lib
  shakoc
)
target_link_directories(hakoniwa-pdu-bridge 
  PRIVATE
    /usr/local/hakoniwa/lib
)
# Enable testing
enable_testing()

# Add our test subdirectory
add_subdirectory(test)

option(HAKO_PDU_BRIDGE_BUILD_EXAMPLES "Build bridge examples" OFF)
if(HAKO_PDU_BRIDGE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Install
install(TARGETS hakoniwa_pdu_bridge_lib
  EXPORT hakoniwa_pdu_bridgeTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/hakoniwa_pdu_bridgeConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/hakoniwa_pdu_bridgeConfig.cmake
  INSTALL_DESTINATION lib/cmake/hakoniwa_pdu_bridge
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/hakoniwa_pdu_bridgeConfigVersion.cmake
  VERSION 1.0.0
  COMPATIBILITY AnyNewerVersion
)

install(EXPORT hakoniwa_pdu_bridgeTargets
  FILE hakoniwa_pdu_bridgeTargets.cmake
  NAMESPACE hakoniwa_pdu_bridge::
  DESTINATION lib/cmake/hakoniwa_pdu_bridge
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/hakoniwa_pdu_bridgeConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/hakoniwa_pdu_bridgeConfigVersion.cmake
  DESTINATION lib/cmake/hakoniwa_pdu_bridge
)
