# KV-Store: A Minimal Log-Structured Merge Tree in C++20

This project implements a simple, educational Log-Structured Merge (LSM) Tree-based key–value store in C++20. It is built from scratch to explore storage engine internals and system-level design.

## Table of Contents

1. Project Goals
2. Core Components
3. Current Progress
4. Write-Ahead Log (WAL) Format
5. SSTable V0 Format
6. Engine Flow
7. Build & Run
8. Project Structure
9. Roadmap

## 1. Project Goals

The goal is to build and understand a minimal LSM-based key–value store by implementing:
- Efficient in-memory indexing (MemTable)
- Crash-safe persistence (WAL)
- Immutable sorted tables (SSTables)
- Coordination logic (Engine)
- Optional features like compaction, bloom filters, and compression

## 2. Core Components

| Component   | Description |
|------------|-------------|
| MemTable    | In-memory sorted map (`std::map`) for fast read/write |
| WAL         | Write-Ahead Log for durability and recovery |
| SSTable     | Immutable, sorted on-disk files created on flush |
| Engine      | Manages WAL, MemTable, and SSTables |
| Optional    | Compaction, bloom filters, compression, checksums, metadata

## 3. Current Progress

### MemTable
- Backed by `std::map`
- Overwrites handled in-place
- Tombstones for deletes
- Size tracking to trigger flush

### WAL (Write-Ahead Log)
- Binary format with `MAGIC` + `VERSION` header
- Supports PUT and DEL records
- Durable via `fsync`
- Replays state on crash
- Supports tail truncation handling and file reset

### SSTable V0
- Sorted, immutable files
- Sparse index (every 64th entry)
- Lookup uses binary search on index and linear scan on data
- Crash-safe via `tmp` write + `fsync` + rename

### Engine Integration
- Writes: WAL → MemTable → flush if needed
- Reads: MemTable → SSTables (newest to oldest)
- Flush: Snapshot → SSTable → WAL reset → MemTable clear
- Recovery: Load SSTables and replay WAL into MemTable

### REPL
Supports the following commands:
```
put <key> <value>
get <key>
del <key>
flush
list
sync
stats
exit
```

## 4. Write-Ahead Log (WAL) Format

```
MAGIC (4B) | VERSION (4B)         -- Header
u32 KeyLen | key bytes
u8 Type    | u32 ValLen
value bytes (if any)
```

- Type: 1 = PUT, 2 = DEL
- DEL should have `ValLen = 0`
- All integers are little-endian
- Truncated tails are ignored during replay

## 5. SSTable V0 Format

```
Header:
  u32 MAGIC = 'KVST'
  u32 VERSION = 1

Data Section (repeated):
  u32 key_len
  u8 type (1=Put, 2=Del)
  u32 value_len
  bytes[key_len] key
  bytes[value_len] value

Sparse Index:
  u32 key_len
  bytes[key_len] key
  u64 file_offset

Footer:
  u64 index_offset
  u32 index_count
  u32 MAGIC = 'KVST'
  u32 VERSION = 1
```

Lookup logic:
- Binary search sparse index
- Seek and scan forward
- Stop on match or greater key

File naming: `000001.sst`, `000002.sst`, ... (monotonically increasing)

## 6. Engine Flow

### Startup
```
open():
  - create data dir
  - load existing SSTables
  - open WAL and replay into MemTable
```

### Write Path
```
put(key, val):
  - WAL.appendPut
  - MemTable.put
  - flush if needed

del(key):
  - WAL.appendDel
  - MemTable.del
  - flush if needed
```

### Read Path
```
get(key):
  - check MemTable (Put/Del)
  - check SSTables newest to oldest
```

### Flush Path
```
flush():
  - snapshot MemTable
  - build SSTable
  - register new SSTable
  - reset WAL
  - clear MemTable
```

## 7. Build & Run

### Requirements
- CMake ≥ 3.15
- C++20 compiler

### Build
```
mkdir -p build
cd build
cmake ..
cmake --build . -j
```

### Run
```
./kv-repl           # REPL shell
./kv-store          # main binary (if present)
./kv-store-tests    # run tests
```

## 8. Project Structure

```
include/          # Public headers
src/              # Implementations
examples/         # REPL shell
tests/            # Unit tests
CMakeLists.txt    # Build configuration
README.md
data/             # Runtime data (wal.log, *.sst)
```

## 9. Roadmap

| Feature            | Status |
|--------------------|--------|
| MemTable           | Done   |
| Write-Ahead Log    | Done   |
| SSTable V0         | Done   |
| Engine Integration | Done   |
| REPL               | Done   |
| Checksums          | TODO   |
| Bloom Filters      | TODO   |
| Compaction         | TODO   |
| Manifest File      | TODO   |
| Compression        | TODO   |