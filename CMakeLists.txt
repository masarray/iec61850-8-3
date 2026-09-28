cmake_minimum_required(VERSION 3.20)
project(iec61850_8_3 VERSION 0.1.0 LANGUAGES CXX)

option(AR61850_DMS_BUILD_TESTS "Build AR61850 DMS tests" ON)
option(AR61850_DMS_BUILD_EXAMPLES "Build AR61850 DMS examples" ON)

add_library(ar61850_dms STATIC
    src/ber.cpp
    src/model.cpp
    src/protocol_decode.cpp
    src/protocol_encode.cpp
    src/server_core.cpp
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

if(AR61850_DMS_BUILD_EXAMPLES)
  add_executable(ar61850_dms_loopback examples/loopback/main.cpp)
  target_link_libraries(ar61850_dms_loopback PRIVATE ar61850::dms)
endif()

if(AR61850_DMS_BUILD_TESTS)
  enable_testing()

  add_executable(ar61850_dms_tests tests/core_tests.cpp)
  target_link_libraries(ar61850_dms_tests PRIVATE ar61850::dms)
  add_test(NAME ar61850_dms_tests COMMAND ar61850_dms_tests)

  add_executable(ar61850_dms_protocol_tests tests/protocol_tests.cpp)
  target_link_libraries(ar61850_dms_protocol_tests PRIVATE ar61850::dms)
  add_test(NAME ar61850_dms_protocol_tests COMMAND ar61850_dms_protocol_tests)
endif()
