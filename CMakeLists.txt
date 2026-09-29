cmake_minimum_required(VERSION 3.20)
project(iec61850_8_3 VERSION 0.1.0 LANGUAGES CXX)

option(AR61850_DMS_BUILD_TESTS "Build AR61850 DMS tests" ON)
option(AR61850_DMS_BUILD_EXAMPLES "Build AR61850 DMS examples" ON)
option(AR61850_DMS_BUILD_APPS "Build AR61850 DMS command-line applications" ON)
option(AR61850_DMS_ENABLE_WEBSOCKET "Build native WebSocket transport" ON)

add_library(ar61850_dms STATIC
    src/ber.cpp
    src/model.cpp
    src/protocol_decode.cpp
    src/protocol_encode.cpp
    src/server_core.cpp
    src/server_runtime.cpp
    src/session.cpp
    src/trace.cpp
    src/worker.cpp
)
add_library(ar61850::dms ALIAS ar61850_dms)

target_compile_features(ar61850_dms PUBLIC cxx_std_20)
target_include_directories(ar61850_dms PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(ar61850_dms PRIVATE /W4 /permissive-)
else()
  target_compile_options(ar61850_dms PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AR61850_DMS_ENABLE_WEBSOCKET)
  include(FetchContent)

  set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
  set(USE_TLS OFF CACHE BOOL "" FORCE)
  set(USE_ZLIB OFF CACHE BOOL "" FORCE)
  set(USE_TEST OFF CACHE BOOL "" FORCE)
  set(USE_WS OFF CACHE BOOL "" FORCE)
  set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG 514a0b968503d758a2954ff9016289f41f489616
  )
  FetchContent_MakeAvailable(ixwebsocket)

  target_sources(ar61850_dms PRIVATE
    src/control_plane.cpp
    src/websocket_transport.cpp
    src/workbench_page.cpp
  )
  target_link_libraries(ar61850_dms PRIVATE ixwebsocket::ixwebsocket)
  target_compile_definitions(ar61850_dms PUBLIC AR61850_DMS_HAS_WEBSOCKET=1)
endif()

if(AR61850_DMS_BUILD_EXAMPLES)
  add_executable(ar61850_dms_loopback examples/loopback/main.cpp)
  target_link_libraries(ar61850_dms_loopback PRIVATE ar61850::dms)
endif()

if(AR61850_DMS_BUILD_APPS AND AR61850_DMS_ENABLE_WEBSOCKET)
  add_executable(ar61850_dms_server apps/dms_server/main.cpp)
  target_link_libraries(ar61850_dms_server PRIVATE ar61850::dms)
endif()

if(AR61850_DMS_BUILD_TESTS)
  enable_testing()

  function(ar61850_enable_test_assertions target)
    if(MSVC)
      target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
  endfunction()

  add_executable(ar61850_dms_tests tests/core_tests.cpp)
  target_link_libraries(ar61850_dms_tests PRIVATE ar61850::dms)
  add_test(NAME ar61850_dms_tests COMMAND ar61850_dms_tests)
  ar61850_enable_test_assertions(ar61850_dms_tests)

  add_executable(ar61850_dms_protocol_tests tests/protocol_tests.cpp)
  target_link_libraries(ar61850_dms_protocol_tests PRIVATE ar61850::dms)
  add_test(NAME ar61850_dms_protocol_tests COMMAND ar61850_dms_protocol_tests)
  ar61850_enable_test_assertions(ar61850_dms_protocol_tests)

  add_executable(ar61850_dms_runtime_tests tests/runtime_tests.cpp)
  target_link_libraries(ar61850_dms_runtime_tests PRIVATE ar61850::dms)
  add_test(NAME ar61850_dms_runtime_tests COMMAND ar61850_dms_runtime_tests)
  ar61850_enable_test_assertions(ar61850_dms_runtime_tests)

  if(AR61850_DMS_ENABLE_WEBSOCKET)
    add_executable(ar61850_dms_websocket_tests tests/websocket_transport_tests.cpp)
    target_link_libraries(ar61850_dms_websocket_tests PRIVATE ar61850::dms)
    add_test(NAME ar61850_dms_websocket_tests COMMAND ar61850_dms_websocket_tests)
  ar61850_enable_test_assertions(ar61850_dms_websocket_tests)

    add_executable(ar61850_dms_control_plane_tests tests/control_plane_tests.cpp)
    target_link_libraries(ar61850_dms_control_plane_tests PRIVATE
      ar61850::dms
      ixwebsocket::ixwebsocket
    )
    add_test(NAME ar61850_dms_control_plane_tests COMMAND ar61850_dms_control_plane_tests)
  ar61850_enable_test_assertions(ar61850_dms_control_plane_tests)
  endif()
endif()
