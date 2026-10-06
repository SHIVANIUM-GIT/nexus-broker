cmake_minimum_required(VERSION 3.14)
project(nexus_broker VERSION 1.0 LANGUAGES CXX)

# We require modern C++20 for high performance features like std::string_view
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release build for high-performance HFT execution
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# HFT Compilation Optimizations:
# -O3: Maximum compiler optimization
# -mno-avx512f: Disable AVX-512 instructions to guarantee container portability
add_compile_options(-O3 -march=x86-64-v3 -mno-avx512f)

# ---------------------------------------------------------
# DEPENDENCIES (Automatically downloaded during build!)
# ---------------------------------------------------------
include(FetchContent)

# 1. cpr (C++ Requests) - For our REST API Connection Pooling
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpr)

# 2. simdjson - The fastest JSON parser in the world (Zero-copy SIMD)
FetchContent_Declare(simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.6.0
)

set(SIMDJSON_IMPLEMENTATION_ICELAKE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(simdjson)

# ---------------------------------------------------------
# BUILD OUR TRADING LIBRARY
# ---------------------------------------------------------
add_library(nexus_broker 
    src/api.cpp
    src/websocket.cpp
    src/oms/RiskManager.cpp
    src/oms/OrderManager.cpp
    src/engine/RealtimeEngine.cpp
    src/engine/OptionSymbolManager.cpp
)

# Tell the compiler where our header files (.hpp) live
target_include_directories(nexus_broker PUBLIC include)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

# Link the blazing fast libraries to our broker
target_link_libraries(nexus_broker PUBLIC
    cpr::cpr
    simdjson::simdjson
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

# ---------------------------------------------------------
# BUILD OUR TRADING ENGINE EXECUTABLE
# ---------------------------------------------------------
add_executable(nexus_engine examples/main.cpp)
target_link_libraries(nexus_engine PRIVATE nexus_broker simdjson::simdjson)

# ---------------------------------------------------------
# TESTS
# ---------------------------------------------------------
add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE nexus_broker simdjson::simdjson)

add_executable(benchmark_engine tests/benchmark_engine.cpp)
target_link_libraries(benchmark_engine PRIVATE nexus_broker simdjson::simdjson)

add_executable(benchmark_real_ws tests/benchmark_real_ws.cpp)
target_link_libraries(benchmark_real_ws PRIVATE nexus_broker simdjson::simdjson)

add_executable(test_token tests/test_token.cpp)
target_link_libraries(test_token PRIVATE nexus_broker)
