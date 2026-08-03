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
option(HAKO_PDU_BRIDGE_BUILD_TCP_TESTS "Build TCP cross-node integration tests" OFF)
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

# Prefer the installed Endpoint CMake package. The package owns its transitive
# dependency contract; Bridge should not reconstruct Core/Boost/nlohmann paths.
find_package(hakoniwa_pdu_endpoint CONFIG QUIET
  HINTS
    "${HAKO_PDU_ENDPOINT_PREFIX}"
    "${HAKO_PDU_ENDPOINT_PREFIX}/lib/cmake/hakoniwa_pdu_endpoint"
)

set(HAKO_USING_ENDPOINT_PACKAGE OFF)
set(HAKO_ENDPOINT_BASE_TARGET "")
set(HAKO_ENDPOINT_CALLBACK_TARGET "")

if(TARGET hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint)
  set(HAKO_USING_ENDPOINT_PACKAGE ON)
  set(HAKO_ENDPOINT_BASE_TARGET hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint)
  if(TARGET hakoniwa_pdu_endpoint::core_callback)
    set(HAKO_ENDPOINT_CALLBACK_TARGET hakoniwa_pdu_endpoint::core_callback)
  endif()
  message(STATUS "Using installed hakoniwa-pdu-endpoint CMake package from ${HAKO_PDU_ENDPOINT_PREFIX}")
endif()

# Compatibility fallback for older Endpoint installations that predate the
# exported package targets. New installations should use the package path above.
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

  add_library(hakoniwa_pdu_endpoint_legacy UNKNOWN IMPORTED)
  set_target_properties(hakoniwa_pdu_endpoint_legacy PROPERTIES
    IMPORTED_LOCATION "${HAKO_PDU_ENDPOINT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
      "${HAKO_PDU_ENDPOINT_INCLUDE_DIR};${HAKO_PDU_ENDPOINT_INCLUDE_DIR}/hakoniwa;${HAKO_PDU_ENDPOINT_INCLUDE_DIR}/hakoniwa/pdu"
    INTERFACE_LINK_LIBRARIES nlohmann_json::nlohmann_json
  )
  set(HAKO_ENDPOINT_BASE_TARGET hakoniwa_pdu_endpoint_legacy)
endif()

if(HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP AND NOT HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE)
  message(FATAL_ERROR "HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP=ON requires HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE=ON")
endif()

# The Hakoniwa-integrated application is callback/asset based. With a modern
# Endpoint package, select that dependency explicitly. Keep a legacy fallback
# for older prefix layouts so direct-CMake users are not broken abruptly.
set(HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS "")
if(HAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE AND HAKO_ENDPOINT_CALLBACK_TARGET STREQUAL "")
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
    list(APPEND HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS "${HAKO_ASSETS_LIBRARY}")
  else()
    list(APPEND HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS assets)
  endif()
  if(HAKO_SHAKOC_LIBRARY)
    list(APPEND HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS "${HAKO_SHAKOC_LIBRARY}")
  else()
    list(APPEND HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS shakoc)
  endif()
  set(HAKO_ENDPOINT_CALLBACK_TARGET ${HAKO_ENDPOINT_BASE_TARGET})
endif()

# Find all our source files
file(GLOB_RECURSE PDU_BRIDGE_SOURCES "src/*.cpp")
set(PDU_BRIDGE_LIB_SOURCES ${PDU_BRIDGE_SOURCES})
list(REMOVE_ITEM PDU_BRIDGE_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/daemon.cpp")
list(REMOVE_ITEM PDU_BRIDGE_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/web_bridge_daemon.cpp")

function(hako_configure_bridge_library target_name endpoint_target)
  add_library(${target_name} ${PDU_BRIDGE_LIB_SOURCES})
  target_include_directories(${target_name}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(${target_name} PUBLIC ${endpoint_target})
endfunction()

# Public Bridge Core library. Its package contract stays Core-free and continues
# to use the base Endpoint target. Existing consumers such as Conductor PRO keep
# seeing this target through the installed Bridge package.
hako_configure_bridge_library(hakoniwa_pdu_bridge_lib ${HAKO_ENDPOINT_BASE_TARGET})
set_target_properties(hakoniwa_pdu_bridge_lib PROPERTIES EXPORT_NAME bridge)

if(HAKO_PDU_BRIDGE_BUILD_STANDALONE_APP)
  add_executable(hakoniwa-pdu-bridge src/daemon.cpp)
  target_link_libraries(hakoniwa-pdu-bridge PRIVATE hakoniwa_pdu_bridge_lib)
endif()

if(HAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP)
  if(HAKO_ENDPOINT_CALLBACK_TARGET STREQUAL "")
    message(FATAL_ERROR
      "Hakoniwa-integrated Bridge requires hakoniwa_pdu_endpoint::core_callback. "
      "Install a Core-enabled hakoniwa-pdu-endpoint package or provide the legacy Core prefix.")
  endif()

  # Do not link the Core-free Bridge library and core_callback Endpoint into the
  # same executable. They contain alternative Endpoint implementations, which
  # causes duplicate/competing linkage on GNU/Linux. Build an internal Bridge
  # variant whose only Endpoint dependency is the callback variant instead.
  hako_configure_bridge_library(hakoniwa_pdu_bridge_core_callback ${HAKO_ENDPOINT_CALLBACK_TARGET})
  if(HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS)
    target_link_libraries(hakoniwa_pdu_bridge_core_callback PUBLIC ${HAKO_PDU_BRIDGE_LEGACY_CORE_LIBS})
  endif()

  add_executable(hakoniwa-pdu-web-bridge src/web_bridge_daemon.cpp)
  target_link_libraries(hakoniwa-pdu-web-bridge PRIVATE hakoniwa_pdu_bridge_core_callback)
  target_include_directories(hakoniwa-pdu-web-bridge PRIVATE
    "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/include"
    "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/include/hakoniwa"
  )

  if(NOT HAKO_USING_ENDPOINT_PACKAGE)
    target_link_directories(hakoniwa-pdu-web-bridge PRIVATE
      "${HAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT}/lib"
    )
  endif()
endif()

if(HAKO_PDU_BRIDGE_BUILD_MONITOR)
  add_executable(hakoniwa-pdu-bridge-monitor tools/bridge_monitor_cli.cpp)
  target_link_libraries(hakoniwa-pdu-bridge-monitor PRIVATE hakoniwa_pdu_bridge_lib)
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
