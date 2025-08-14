# KV-Store

A simple **Log-Structured Merge (LSM) Tree** style key–value store built from scratch in C++20.
This is a learning-focused project — implemented step-by-step to understand modern storage engine internals.

## Goals
Understand and implement the core components of an LSM-based KV store:
1. **MemTable** – in-memory sorted map for fast writes.
2. **Write-Ahead Log (WAL)** – crash recovery.
3. **SSTable** – immutable sorted on-disk data files.
4. **Engine** – coordinates MemTable, WAL, and SSTables.
5. (Optional) Compaction, bloom filters, manifest file, checksums, compression.

---

## ✅ Current Progress
### **MemTable**
- In-memory, ordered (`std::map`) store of recent writes.
- Overwrites handled in-place.
- Tombstones represent deletes (`RecType::Del`).
- Rough size tracking (`bytes()`) used for flush triggers.

### **Write-Ahead Log (WAL)**
- Append-only binary log with fixed header (MAGIC + VERSION).
- PUT and DEL record types.
- Crash-safe replay with truncated-tail tolerance.
- `reset()` to truncate WAL after a successful MemTable → SSTable flush.
- Tested behaviors:
  - New vs. existing file handling.
  - Append → sync → replay (idempotent).
  - Truncated tail recovery.
  - Large key/value handling.

### **SSTable (V0)**
- Immutable, **sorted** on-disk file created on flush.
- Simple **sparse index** (every 64th entry) stored at the end.
- Point lookups: binary-search index ➜ short linear scan in data section.
- Newer SSTables shadow older ones; tombstones hide older puts.
- Crash safety: write to `tmp_XXXXXX.sst`, `fsync()`, `fsync(dir)`, then `rename()`.

### **Engine Integration**
- **Write path**: `put/del` → append to WAL → apply to MemTable → flush if threshold exceeded.
- **Read path**: MemTable first ➜ SSTables **newest → oldest** (stop on tombstone; first put wins).
- **Flush path**: snapshot MemTable (already sorted), build SSTable, open & register newest-first, `wal.reset()`, `mem.clear()`.
- **Recovery** on startup: load SSTables (newest-first), open WAL, replay WAL into MemTable.

### **REPL**
- Minimal interactive shell for manual testing: `put/get/del/flush/list/sync/stats/exit`.

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
- All integers are little-endian (u32 unless noted).
- DEL records should have `ValLen = 0` (parser tolerates non‑zero by skipping bytes).
- Truncated tails during replay are handled gracefully.

---

## 💽 SSTable (V0) File Layout

**Header**
```
u32 MAGIC = 'KVST' (0x4B565354)
u32 VERSION = 1
```

**Data section** (repeated, keys strictly increasing)
```
u32 key_len
u8  type              # 1=Put, 2=Del
u32 value_len         # 0 for Del
bytes[key_len]   key
bytes[value_len] value
```

**Sparse index** (every 64th entry by default)
```
u32 key_len
bytes[key_len] key
u64 file_offset   # absolute offset of the indexed entry in the data section
```

**Footer (fixed size)**
```
u64 index_offset
u32 index_count
u32 MAGIC = 'KVST'
u32 VERSION = 1
```

**Lookup algorithm**
1) Binary-search the in-memory sparse index to find the greatest index key ≤ target.  
2) Seek to that offset and linearly scan forward:
   - If `key == target`:
     - `Put` → return value
     - `Del` → treat as not found (tombstone)
   - If `key > target` → stop (not found in this file).

**File naming**
- `NNNNNN.sst` (zero-padded, monotonically increasing). Higher ID = newer.

---

## 🧠 Engine Flow

**Startup / Recovery**
```
open():
  create data dir if needed
  load_existing_sstables()      # scan *.sst, newest → oldest
  wal.open()
  reader.replay(mem)            # WAL → MemTable
```

**Write path**
```
put(key,val):
  wal.appendPut(key,val)
  mem.put(key,val)
  flush_if_needed()

del(key):
  wal.appendDel(key)
  mem.del(key)
  flush_if_needed()
```

**Read path**
```
get(key):
  if mem has Put → return value
  if mem has Del → return not found
  for each SSTable (newest → oldest):
     probe:
       Put       → return value
       Tombstone → return not found
       Absent    → continue
  return not found
```

**Flush path**
```
flush():
  snapshot mem (sorted)
  build SSTable (tmp → fsync → fsync(dir) → rename)
  open & register table as newest
  wal.reset()
  mem.clear()
```

---

## 🧪 Build & Run

### Prereqs
- CMake ≥ 3.15
- A C++20 compiler (clang++/g++)

### Build
```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
```

### Run the REPL
```bash
./kv-repl
```
**Commands**
```
put <key> <value...>
get <key>
del <key>
flush            # force MemTable → SSTable
list             # show SSTables (newest → oldest)
sync             # fsync WAL
stats            # mem.size / mem.bytes
exit | quit
```

### Run the original binary (if present)
```bash
./kv-store
```

### Run tests
```bash
./kv-store-tests
```

> Binaries may differ if you’ve renamed targets in your CMake; see `CMakeLists.txt`.

---

## Project Structure
```
include/            # Public headers (memtable.h, wal.h, sstable.h, engine.h)
src/                # Implementations (.cpp)
examples/           # kv_repl.cpp (interactive shell)
tests/              # Unit tests
CMakeLists.txt      # Build configuration
README.md
data/               # Runtime data (wal.log, *.sst) - created at run time
```

---

## Roadmap
- [x] MemTable
- [x] WAL
- [x] SSTable (V0: sparse index, no checksums/compression)
- [x] Engine integration (WAL + Mem + SSTables)
- [x] REPL
- [ ] Checksums (per-block or per-entry), error handling improvements
- [ ] Bloom filters (global or per-block) to skip cold files
- [ ] Compaction (merge & tombstone GC)
- [ ] Manifest/CURRENT file & metadata
- [ ] Compression (LZ4/Snappy) & prefix-compressed blocks
