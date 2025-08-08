#include "engine.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

Engine::Engine(std::string data_dir)
    : dir_(std::move(data_dir)),
      wal_path_(dir_ + "/wal.log"),
      snapshot_path_(dir_ + "/snapshot.bin") {
    std::filesystem::create_directories(dir_);
    recover();
}

void Engine::put(const std::string& key, const std::string& value) {
    append_record(Engine::RecType::Put, key, value);
    mem_[key] = value;
}

void Engine::del(const std::string& key) {
    append_record(Engine::RecType::Del, key, "");
    mem_.erase(key);
}

std::string Engine::get(const std::string& key) const {
    auto it = mem_.find(key);
    return it == mem_.end() ? std::string{} : it->second;
}

void Engine::snapshot() {
    // Write temp snapshot: [count:u32] then entries [klen:u32][vlen:u32][k][v]
    std::string tmp = snapshot_path_ + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("failed to open temp snapshot");
        write_u32(out, static_cast<uint32_t>(mem_.size()));
        for (const auto& kv : mem_) {
            const auto& k = kv.first;
            const auto& v = kv.second;
            write_u32(out, static_cast<uint32_t>(k.size()));
            write_u32(out, static_cast<uint32_t>(v.size()));
            if (!k.empty()) out.write(k.data(), k.size());
            if (!v.empty()) out.write(v.data(), v.size());
        }
        out.flush();
    }
    std::filesystem::rename(tmp, snapshot_path_);

    // Truncate WAL
    std::ofstream trunc(wal_path_, std::ios::binary | std::ios::trunc);
    trunc.flush();
}

void Engine::write_u32(std::ofstream& f, uint32_t x) {
    f.write(reinterpret_cast<const char*>(&x), sizeof(x));
}
uint32_t Engine::read_u32(std::ifstream& f) {
    uint32_t x = 0;
    f.read(reinterpret_cast<char*>(&x), sizeof(x));
    return x;
}

void Engine::append_record(RecType ty, const std::string& k, const std::string& v) {
    std::ofstream f(wal_path_, std::ios::binary | std::ios::app);
    if (!f) throw std::runtime_error("open wal for append failed");
    // record: len (u32) = 1 + 4 + 4 + k.size + v.size
    uint32_t len = 1 + 4 + 4 + static_cast<uint32_t>(k.size()) + static_cast<uint32_t>(v.size());
    write_u32(f, len);
    uint8_t t = static_cast<uint8_t>(ty);
    f.write(reinterpret_cast<const char*>(&t), 1);
    write_u32(f, static_cast<uint32_t>(k.size()));
    write_u32(f, static_cast<uint32_t>(v.size()));
    if (!k.empty()) f.write(k.data(), k.size());
    if (!v.empty()) f.write(v.data(), v.size());
    f.flush(); // dev-grade durability; can add fsync later
}

void Engine::recover() {
    // 1) Load snapshot first
    if (std::filesystem::exists(snapshot_path_)) {
        std::ifstream in(snapshot_path_, std::ios::binary);
        if (in) {
            uint32_t count = read_u32(in);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t klen = read_u32(in);
                uint32_t vlen = read_u32(in);
                std::string k(klen, '\0');
                std::string v(vlen, '\0');
                if (klen) in.read(k.data(), klen);
                if (vlen) in.read(v.data(), vlen);
                if (!in) break;
                mem_[std::move(k)] = std::move(v);
            }
        }
    }

    // 2) Apply WAL tail
    std::ifstream f(wal_path_, std::ios::binary);
    if (!f) return;

    while (true) {
        uint32_t len = 0;
        f.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!f) break; // EOF or truncated

        if (len < 1 + 4 + 4) break;
        uint8_t ty_u8 = 0;
        f.read(reinterpret_cast<char*>(&ty_u8), 1);
        uint32_t klen = read_u32(f);
        uint32_t vlen = read_u32(f);

        uint64_t required = static_cast<uint64_t>(klen) + static_cast<uint64_t>(vlen);
        if (required != (len - (1 + 4 + 4))) break; // tail trunc

        std::string k(klen, '\0');
        if (klen) f.read(k.data(), klen);
        std::string v(vlen, '\0');
        if (vlen) f.read(v.data(), vlen);
        if (!f) break;

        if (static_cast<RecType>(ty_u8) == Engine::RecType::Put) {
            mem_[k] = v;
        } else if (static_cast<RecType>(ty_u8) == Engine::RecType::Del) {
            mem_.erase(k);
        } else {
            break;
        }
    }
}
