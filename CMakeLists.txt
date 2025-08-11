cmake_minimum_required(VERSION 3.15)
project(kv_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -O2)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(kv-store
    src/main.cpp
    src/memtable.cpp
    src/wal.cpp
)

add_executable(kv-store-tests
    tests/wal_tests.cpp
    src/memtable.cpp
    src/wal.cpp
)
target_include_directories(kv-store-tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

