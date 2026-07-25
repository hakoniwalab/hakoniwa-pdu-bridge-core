cmake_minimum_required(VERSION 3.16)
project(hakoniwa-pdu-bridge CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakePackageConfigHelpers)

# Keep legacy direct-CMake behavior by default. The manifest-driven flow passes
# these options explicitly so its result does not depend on historical defaults.
option(HAKO_PDU_BRIDGE_BUILD_STANDALONE_APP "Build standalone bridge daemon" ON)
option(HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP "Build Hakoniwa-integrated web bridge daemon" ON)
option(HAKO_PDU_BRIDGE_BUILD_MONITOR "Build bridge monitor CLI" ON)
option(HAKO_PDU_BRIDGE_BUILD_TESTS "Build bridge tests" ON)
option(HAKO_PDU_BRIDGE_BUILD_EXAMPLES "Build bridge examples" ON)
option(HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE "Resolve/link Hakoniwa Core runtime libraries" ON)

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
set(HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT "${HAKO_PDU_ENDPOINT_PREFIX}" CACHE PATH
    "Install prefix of Hakoniwa Core used by Hakoniwa-integrated applications")
set(HAKO_PDU_REGISTRY_ROOT "" CACHE PATH
    "Source checkout of hakoniwa-pdu-registry used by Bridge tests")
if(HAKO_PDU_REGISTRY_ROOT STREQUAL "" AND DEFINED ENV{HAKO_PDU_REGISTRY_ROOT})
  set(HAKO_PDU_REGISTRY_ROOT "$ENV{HAKO_PDU_REGISTRY_ROOT}" CACHE PATH
      "Source checkout of hakoniwa-pdu-registry used by Bridge tests" FORCE)
endif()

# Prefer the installed Endpoint CMake package. This preserves public usage
# requirements such as HAKO_PDU_ENDPOINT_DISABLE_HAKONIWA_CORE and transitive
# include/link requirements. Fall back to legacy header/library discovery for
# older Endpoint installations that do not provide a package config.
find_package(hakoniwa_pdu_endpoint CONFIG QUIET
  HINTS
    "${HAKO_PDU_ENDPOINT_PREFIX}"
    "${HAKO_PDU_ENDPOINT_PREFIX}/lib/cmake/hakoniwa_pdu_endpoint"
)

set(HAKO_USING_ENDPOINT_PACKAGE OFF)
if(TARGET hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint)
  set(HAKO_USING_ENDPOINT_PACKAGE ON)
  if(NOT TARGET hakoniwa_pdu_endpoint)
    add_library(hakoniwa_pdu_endpoint ALIAS hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint)
  endif()
  message(STATUS "Using installed hakoniwa-pdu-endpoint CMake package from ${HAKO_PDU_ENDPOINT_PREFIX}")
endif()

set(HAKO_ENDPOINT_EXTRA_LIBS nlohmann_json::nlohmann_json)
set(HAKO_PDU_BRIDGE_CORE_LIBS "")

if(NOT HAKO_USING_ENDPOINT_PACKAGE)
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
      "${HAKO_PDU_ENDPOINT_PREFIX}/bin"
      "/usr/local/lib"
      "/usr/local/hakoniwa/lib"
  )

  if(NOT HAKO_PDU_ENDPOINT_INCLUDE_DIR)
    message(FATAL_ERROR "hakoniwa-pdu-endpoint header not found. Set HAKO_PDU_ENDPOINT_INCLUDE_DIR or HAKO_PDU_ENDPOINT_PREFIX.")
  endif()
  if(NOT HAKO_PDU_ENDPOINT_LIBRARY)
    message(FATAL_ERROR "hakoniwa-pdu-endpoint library not found. Set HAKO_PDU_ENDPOINT_LIBRARY or HAKO_PDU_ENDPOINT_PREFIX.")
  endif()
endif()

if(HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
  find_library(HAKO_ASSETS_LIBRARY
    NAMES assets
    HINTS
      "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib"
      "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/bin"
      "/usr/local/hakoniwa/lib"
      "/usr/local/lib"
  )
  find_library(HAKO_SHAKOC_LIBRARY
    NAMES shakoc
    HINTS
      "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib"
      "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/bin"
      "/usr/local/hakoniwa/lib"
      "/usr/local/lib"
  )

  if(HAKO_ASSETS_LIBRARY)
    list(APPEND HAKO_ENDPOINT_EXTRA_LIBS "${HAKO_ASSETS_LIBRARY}")
    list(APPEND HAKO_PDU_BRIDGE_CORE_LIBS "${HAKO_ASSETS_LIBRARY}")
  else()
    list(APPEND HAKO_ENDPOINT_EXTRA_LIBS assets)
    list(APPEND HAKO_PDU_BRIDGE_CORE_LIBS assets)
  endif()
  if(HAKO_SHAKOC_LIBRARY)
    list(APPEND HAKO_ENDPOINT_EXTRA_LIBS "${HAKO_SHAKOC_LIBRARY}")
    list(APPEND HAKO_PDU_BRIDGE_CORE_LIBS "${HAKO_SHAKOC_LIBRARY}")
  else()
    list(APPEND HAKO_ENDPOINT_EXTRA_LIBS shakoc)
    list(APPEND HAKO_PDU_BRIDGE_CORE_LIBS shakoc)
  endif()
endif()

if(HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP AND NOT HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
  message(FATAL_ERROR "HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP=ON requires HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE=ON")
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
list(REMOVE_ITEM PDU_BRIDGE_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/web_bridge_daemon.cpp")

# Bridge Core library. Its primary runtime dependency is hakoniwa-pdu-endpoint.
add_library(hakoniwa_pdu_bridge_lib ${PDU_BRIDGE_LIB_SOURCES})
target_include_directories(hakoniwa_pdu_bridge_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(hakoniwa_pdu_bridge_lib PUBLIC hakoniwa_pdu_endpoint)

# Preserve the historical Apple include search only for Core-enabled builds.
if(APPLE AND HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
  target_include_directories(hakoniwa_pdu_bridge_lib
    PRIVATE
      "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/include/hakoniwa"
  )
endif()

if(HAKO_PDU_BRIDGE_BUILD_STANDALONE_APP)
  add_executable(hakoniwa-pdu-bridge src/daemon.cpp)
  target_link_libraries(hakoniwa-pdu-bridge PRIVATE hakoniwa_pdu_bridge_lib)
endif()

if(HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP)
  add_executable(hakoniwa-pdu-web-bridge src/web_bridge_daemon.cpp)
  target_include_directories(hakoniwa-pdu-web-bridge PRIVATE
    "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/include"
    "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/include/hakoniwa"
  )
  target_link_libraries(hakoniwa-pdu-web-bridge
    PRIVATE
      hakoniwa_pdu_bridge_lib
      ${HAKO_PDU_BRIDGE_CORE_LIBS}
  )
endif()

if(HAKO_PDU_BRIDGE_BUILD_MONITOR)
  add_executable(hakoniwa-pdu-bridge-monitor tools/bridge_monitor_cli.cpp)
  target_link_libraries(hakoniwa-pdu-bridge-monitor PRIVATE hakoniwa_pdu_bridge_lib)
endif()

# Legacy install prefixes remain valid for direct CMake users. New manifest builds
# resolve concrete dependency roots and pass them explicitly.
if(HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
  if(TARGET hakoniwa-pdu-bridge)
    target_link_directories(hakoniwa-pdu-bridge PRIVATE "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib")
  endif()
  if(TARGET hakoniwa-pdu-web-bridge)
    target_link_directories(hakoniwa-pdu-web-bridge PRIVATE "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib")
  endif()
  if(TARGET hakoniwa-pdu-bridge-monitor)
    target_link_directories(hakoniwa-pdu-bridge-monitor PRIVATE "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib")
  endif()
endif()

if(HAKO_PDU_BRIDGE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

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
if(TARGET hakoniwa-pdu-bridge)
  install(TARGETS hakoniwa-pdu-bridge RUNTIME DESTINATION bin)
endif()
if(TARGET hakoniwa-pdu-web-bridge)
  install(TARGETS hakoniwa-pdu-web-bridge RUNTIME DESTINATION bin)
endif()
if(TARGET hakoniwa-pdu-bridge-monitor)
  install(TARGETS hakoniwa-pdu-bridge-monitor RUNTIME DESTINATION bin)
endif()
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config/web_bridge/ DESTINATION share/hakoniwa-pdu-bridge/config/web_bridge)
install(DIRECTORY config/web_bridge_fleets/ DESTINATION share/hakoniwa-pdu-bridge/config/web_bridge_fleets)
install(DIRECTORY config/web_bridge_game/ DESTINATION share/hakoniwa-pdu-bridge/config/web_bridge_game)
install(DIRECTORY config/web_bridge_disturb/ DESTINATION share/hakoniwa-pdu-bridge/config/web_bridge_disturb)
install(PROGRAMS tools/run-web-bridge.bash DESTINATION bin)

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
