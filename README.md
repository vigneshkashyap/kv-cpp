# kvx-mini (Stage 2)
Minimal C++17 key-value store with:
- Write-Ahead Log (WAL)
- Full snapshots (snapshot.bin)
- Simple REPL (PUT/GET/DEL/SNAPSHOT/EXIT)

## Build (no CMake needed)
```bash
make
./kvx
# or choose a data directory
./kvx ./my_data_dir
```

## Commands
- `PUT <key> <value...>`
- `GET <key>`
- `DEL <key>`
- `SNAPSHOT`
- `EXIT`
