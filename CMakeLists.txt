#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

cmake_minimum_required(VERSION 3.24)

project(Selectors LANGUAGES C CXX VERSION 0.1)

include(CTest)
include(GenerateExportHeader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

add_library(selectors SHARED SelectorExpression.cpp SelectorToken.cpp SelectorValue.cpp selectors.cpp)
set_target_properties(selectors
    PROPERTIES
        INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        INTERPROCEDURAL_OPTIMIZATION on)

generate_export_header(selectors)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Readline IMPORTED_TARGET readline)
endif(PKG_CONFIG_FOUND)
set(found_readline "$<BOOL:${Readline_FOUND}>")

add_executable(selector_cli selector_cli.c)
target_link_libraries(selector_cli PRIVATE selectors $<${found_readline}:PkgConfig::Readline>)
set_target_properties(selector_cli
  PROPERTIES
    INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
    COMPILE_DEFINITIONS $<${found_readline}:READLINE>)

find_package(Catch2)
if(Catch2_FOUND)
  include(Catch)

  add_executable(selector_tests SelectorTests.cpp)
  target_link_libraries(selector_tests
    PRIVATE
      selectors
      $<IF:$<TARGET_EXISTS:Catch2::Catch2WithMain>, Catch2::Catch2WithMain, Catch2::Catch2>)
  set_target_properties(selector_tests
    PROPERTIES
      INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

  catch_discover_tests(selector_tests)
endif(Catch2_FOUND)

add_executable(selector_regression_test
    selector_fuzzer.cpp
    selector_fuzzer_main.cpp)
target_link_libraries(selector_regression_test PRIVATE selectors)
set_target_properties(selector_regression_test
    PROPERTIES
        INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

# Define option to enable fuzzing in an interface library
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_library(fuzzing_options INTERFACE)
    target_compile_options(fuzzing_options INTERFACE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
    target_link_options(fuzzing_options INTERFACE
        -fsanitize=fuzzer,address,undefined)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "")
endif()

# Optional fuzzing support
option(ENABLE_FUZZER "Build with fuzzing instrumentation and create static fuzz target" OFF)

# Require Clang for fuzzing builds
if(ENABLE_FUZZER AND NOT TARGET fuzzing_options)
    message(FATAL_ERROR "ENABLE_FUZZER requires Clang compiler. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if(ENABLE_FUZZER)
    # Create static library with fuzzing instrumentation
    add_library(selectors-fuzz STATIC $<TARGET_PROPERTY:selectors,SOURCES>)
    target_link_libraries(selectors-fuzz PUBLIC fuzzing_options)
    set_target_properties(selectors-fuzz PROPERTIES
        INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
        INTERPROCEDURAL_OPTIMIZATION ON)

    # Fuzzer executable - just the fuzzer entry point, no main
    add_executable(selector_fuzzer selector_fuzzer.cpp)
    target_link_libraries(selector_fuzzer PRIVATE selectors-fuzz)
    set_target_properties(selector_fuzzer PROPERTIES
        INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
        INTERPROCEDURAL_OPTIMIZATION ON)
endif()

