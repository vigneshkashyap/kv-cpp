cmake_minimum_required(VERSION 3.15)
project(kv_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

find_package(ZLIB REQUIRED)

add_library(kv_store_core STATIC
    src/memtable.cpp
    src/wal.cpp
    src/sstable.cpp
    src/engine.cpp
    src/utils.cpp
)
target_include_directories(kv_store_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(kv_store_core PUBLIC ZLIB::ZLIB)

# Main CLI tool
add_executable(kv-store
    src/main.cpp
)
target_link_libraries(kv-store PRIVATE kv_store_core)

# REPL
add_executable(kv-repl
    examples/kv_repl.cpp
)
target_link_libraries(kv-repl PRIVATE kv_store_core)

# Tests
add_executable(kv-store-tests
    tests/wal_tests.cpp
)
target_link_libraries(kv-store-tests PRIVATE kv_store_core)
