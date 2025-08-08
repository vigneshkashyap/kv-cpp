# KV-Mini
Minimal C++17 key-value store with:
- Write-Ahead Log (WAL)
- Full snapshots (snapshot.bin)
- Simple REPL (PUT/GET/DEL/SNAPSHOT/EXIT)

## Build (no CMake needed)
```bash
make
./kv
# or choose a data directory
./kv ./my_data_dir
```

## Commands
- `PUT <key> <value...>`
- `GET <key>`
- `DEL <key>`
- `SNAPSHOT`
- `EXIT`
