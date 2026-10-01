cmake_minimum_required(VERSION 3.15)
project(cpu_sim VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion)
endif()

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
)

# Main executable
add_executable(cpu-sim ${SIM_SOURCES})
target_compile_features(cpu-sim PRIVATE cxx_std_17)
set_target_properties(cpu-sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Unit tests
if(BUILD_TESTS)
    enable_testing()
    
    # Fetch Catch2
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.3
    )
    FetchContent_MakeAvailable(Catch2)
    
    # Test executable
    add_executable(unit_tests
        src/test_main.cpp
        src/arch/Pipeline.cpp
        src/arch/Cache.cpp
        src/arch/BranchPredictor.cpp
        src/sim/Simulator.cpp
        src/workload/MatMulWorkload.cpp
        src/workload/Conv1DWorkload.cpp
    )
    target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain)
    target_compile_features(unit_tests PRIVATE cxx_std_17)
    
    add_test(NAME unit_tests COMMAND unit_tests)
    
    # Integration tests (run simulator and verify output)
    add_test(NAME integration_matmul_32kb
        COMMAND cpu-sim 32768
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
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