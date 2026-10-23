cmake_minimum_required(VERSION 3.15)
project(cpu_sim VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit tests" OFF)

# Interface target for strict warnings (applied only to project targets, not dependencies)
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /WX
        /permissive-
    >
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SIM_SOURCES
    src/main.cpp
    src/arch/Pipeline.cpp
    src/arch/Cache.cpp
    src/arch/BranchPredictor.cpp
    src/sim/Simulator.cpp
    src/workload/MatMulWorkload.cpp
    src/workload/Conv1DWorkload.cpp
    src/workload/BranchWorkload.cpp
)

# Main executable
add_executable(cpu-sim ${SIM_SOURCES})
target_compile_features(cpu-sim PRIVATE cxx_std_17)
target_link_libraries(cpu-sim PRIVATE project_warnings)
set_target_properties(cpu-sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Unit tests
if(BUILD_TESTS)
    enable_testing()
    add_executable(unit_tests
        src/test_main.cpp
        src/arch/Pipeline.cpp
        src/arch/Cache.cpp
        src/arch/BranchPredictor.cpp
        src/sim/Simulator.cpp
        src/workload/MatMulWorkload.cpp
        src/workload/Conv1DWorkload.cpp
        src/workload/BranchWorkload.cpp
    )
    target_compile_features(unit_tests PRIVATE cxx_std_17)
    target_link_libraries(unit_tests PRIVATE project_warnings)
    
    add_test(NAME unit_tests COMMAND unit_tests)
    
    add_test(NAME integration_matmul_32kb
        COMMAND cpu-sim --workload matmul --matmul-n 2 --cache-size 32768 --json)
    set_tests_properties(integration_matmul_32kb PROPERTIES
        PASS_REGULAR_EXPRESSION "\"instructions_retired\": 32")
    add_test(NAME integration_branch
        COMMAND cpu-sim --workload branch --branch-count 3 --bp-accuracy 0 --json)
    set_tests_properties(integration_branch PROPERTIES
        PASS_REGULAR_EXPRESSION "\"instructions_retired\": 6")
    add_test(NAME integration_config
        COMMAND cpu-sim --config ${CMAKE_CURRENT_SOURCE_DIR}/tests/conv1d_config.json)
    set_tests_properties(integration_config PROPERTIES
        PASS_REGULAR_EXPRESSION "\"instructions_retired\": 24")
    add_test(NAME reject_invalid_config
        COMMAND cpu-sim --config ${CMAKE_CURRENT_SOURCE_DIR}/tests/invalid_config.json)
    set_tests_properties(reject_invalid_config PROPERTIES WILL_FAIL TRUE)
    add_test(NAME reject_width_nine COMMAND cpu-sim --issue-width 9)
    set_tests_properties(reject_width_nine PROPERTIES WILL_FAIL TRUE)
    add_test(NAME reject_nan_accuracy COMMAND cpu-sim --bp-accuracy nan)
    set_tests_properties(reject_nan_accuracy PROPERTIES WILL_FAIL TRUE)
    add_test(NAME reject_conv_kernel COMMAND cpu-sim --workload conv1d --conv1d-len 3 --conv1d-kernel 4)
    set_tests_properties(reject_conv_kernel PROPERTIES WILL_FAIL TRUE)
    add_test(NAME reject_zero_progress COMMAND cpu-sim --progress --progress-interval 0)
    set_tests_properties(reject_zero_progress PROPERTIES WILL_FAIL TRUE)
    add_test(NAME reject_overflowing_cache_geometry
        COMMAND cpu-sim --cache-size 128 --cache-line-size 2147483648 --cache-assoc 2)
    set_tests_properties(reject_overflowing_cache_geometry PROPERTIES WILL_FAIL TRUE)
endif()

# Installation
install(TARGETS cpu-sim
    RUNTIME DESTINATION bin
)

# Package configuration
include(CPack)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
