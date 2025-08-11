# KV-Store (Learning Project)

A simple **Log-Structured Merge (LSM) Tree** style key–value store built from scratch in C++20.
This is a learning-focused project — implemented step-by-step to understand modern storage engine internals.

## Goals
- Understand and implement the core components of an LSM-based KV store:
  1. **MemTable** – in-memory sorted map for fast writes.
  2. **Write-Ahead Log (WAL)** – crash recovery.
  3. **SSTable** – immutable sorted on-disk data files.
  4. **Engine** – coordinates MemTable, WAL, and SSTables.
  5. (Optional) Compaction, bloom filters, manifest file.

**MemTable implemented and tested**
Stores recent writes in memory, ordered by key, with tombstones for deletes.

## Build & Run
```bash
mkdir -p build
cd build
cmake ..
make
./kv-store
