# KV-Store

A simple **Log-Structured Merge (LSM) Tree** style key–value store built from scratch in C++20.
This is a learning-focused project — implemented step-by-step to understand modern storage engine internals.

## Goals
- Understand and implement the core components of an LSM-based KV store:
  1. **MemTable** – in-memory sorted map for fast writes.
  2. **Write-Ahead Log (WAL)** – crash recovery.
  3. **SSTable** – immutable sorted on-disk data files.
  4. **Engine** – coordinates MemTable, WAL, and SSTables.
  5. (Optional) Compaction, bloom filters, manifest file.

---

## ✅ Current Progress
### **MemTable**
- In-memory, ordered (`std::map`) store of recent writes.
- Overwrites handled in-place.
- Tombstones used for deletes.
- Approximate size tracking for flush triggers.

### **Write-Ahead Log (WAL)**
- Append-only binary log with fixed header (MAGIC + VERSION).
- PUT and DEL record types.
- Crash-safe replay with truncated-tail tolerance.
- `reset()` support to truncate WAL after MemTable flush.
- Fully tested with:
  - New vs existing file handling.
  - Append → sync → replay.
  - Truncated tail recovery.
  - Idempotent replays.
  - Large key/value handling.

---

## 📜 WAL File Layout

```
+-----------------+------------------+
|  MAGIC (4B)     | VERSION (4B)     |   <-- File header
+-----------------+------------------+
|  u32 KeyLen     | key bytes (...)  |
+-----------------+------------------+
|  u8  Type       | u32 ValLen       |   <-- Type: 1=PUT, 2=DEL
+-----------------+------------------+
|  value bytes (...)                 |
+------------------------------------+
|  (repeat for each record)          |
+------------------------------------+
```

**Notes**
- All integers are little-endian (`u32` unless noted).
- DEL records should have `ValLen = 0` (tolerated if not).
- Truncated tails during replay are handled gracefully.

---

## Build & Run
```bash
mkdir -p build
cd build
cmake ..
make
./kv-store
```

### Run Tests
```bash
./kv-store-tests
```

---

## Project Structure
```
include/        # Public headers
src/            # Implementation (.cpp files)
tests/          # Unit tests
CMakeLists.txt  # Build configuration
README.md
```

---

## Roadmap
- [x] MemTable
- [x] WAL
- [ ] SSTable
- [ ] Engine integration
- [ ] Compaction
