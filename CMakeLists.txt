cmake_minimum_required(VERSION 3.15)
project(kv_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

# Core library: share MemTable/WAL/SSTable/Engine across all executables
add_library(kv_store_core STATIC
    src/memtable.cpp
    src/wal.cpp
    src/sstable.cpp
    src/engine.cpp
)
target_include_directories(kv_store_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Your original engine binary (keeps your src/main.cpp)
add_executable(kv-store
    src/main.cpp
)
target_link_libraries(kv-store PRIVATE kv_store_core)

# REPL (interactive shell)
add_executable(kv-repl
    examples/kv_repl.cpp
)
target_link_libraries(kv-repl PRIVATE kv_store_core)

# Tests (now link the core lib; don't re-list memtable/wal sources here)
add_executable(kv-store-tests
    tests/wal_tests.cpp
)
target_link_libraries(kv-store-tests PRIVATE kv_store_core)
