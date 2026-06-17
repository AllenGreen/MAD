#include "core/serialize.hpp"
#include "core/log.hpp"

#include <cstring>
#include <sstream>
#include <string>

namespace mad::core {

namespace {

constexpr uint32_t MAGIC = 0x4D414453; // "MADS"
constexpr uint32_t VERSION = 1;

template <typename T>
void put(std::vector<uint8_t>& b, const T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    b.insert(b.end(), p, p + sizeof(T));
}

void put_str(std::vector<uint8_t>& b, const std::string& s) {
    put<uint32_t>(b, static_cast<uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

struct Reader {
    std::span<const uint8_t> data;
    size_t pos = 0;
    bool ok = true;

    template <typename T>
    T get() {
        T v{};
        if (pos + sizeof(T) > data.size()) { ok = false; return v; }
        std::memcpy(&v, data.data() + pos, sizeof(T));
        pos += sizeof(T);
        return v;
    }
    std::string get_str() {
        const auto len = get<uint32_t>();
        if (!ok || pos + len > data.size()) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(data.data() + pos), len);
        pos += len;
        return s;
    }
};

} // namespace

std::vector<uint8_t> serialize_world(const World& w) {
    std::vector<uint8_t> b;
    put<uint32_t>(b, MAGIC);
    put<uint32_t>(b, VERSION);
    put<uint64_t>(b, w.tick_);
    put<uint32_t>(b, w.next_demon_id_);
    put<int32_t>(b, w.reached_nexus_count_);

    std::ostringstream rng;
    rng << w.rng_;
    put_str(b, rng.str());

    put<uint32_t>(b, static_cast<uint32_t>(w.demons_.size()));
    for (const Demon& d : w.demons_) {
        put<uint32_t>(b, d.id);
        put<double>(b, d.pos.x);
        put<double>(b, d.pos.y);
        put<uint8_t>(b, static_cast<uint8_t>(d.move_type));
        put<int32_t>(b, d.size);
        put<double>(b, d.speed);
        put<double>(b, d.hp);
        put<double>(b, d.max_hp);
        put<int32_t>(b, d.spawn_sector);
        put<int32_t>(b, d.sector);
        put<int32_t>(b, d.wave_dir);
        put<int32_t>(b, d.shards_collected);
        put<uint8_t>(b, d.reached_nexus ? 1 : 0);
        put<uint8_t>(b, d.alive ? 1 : 0);
    }
    return b;
}

bool deserialize_world(World& w, std::span<const uint8_t> bytes) {
    Reader r{bytes};
    if (r.get<uint32_t>() != MAGIC) {
        log::error("Serialize", "bad magic");
        return false;
    }
    if (r.get<uint32_t>() != VERSION) {
        log::error("Serialize", "version mismatch");
        return false;
    }
    w.tick_ = r.get<uint64_t>();
    w.next_demon_id_ = r.get<uint32_t>();
    w.reached_nexus_count_ = r.get<int32_t>();

    std::istringstream rng(r.get_str());
    rng >> w.rng_;

    const auto count = r.get<uint32_t>();
    if (!r.ok) return false;
    w.demons_.clear();
    w.demons_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Demon d;
        d.id = r.get<uint32_t>();
        d.pos.x = r.get<double>();
        d.pos.y = r.get<double>();
        d.move_type = static_cast<game::MoveType>(r.get<uint8_t>());
        d.size = r.get<int32_t>();
        d.speed = r.get<double>();
        d.hp = r.get<double>();
        d.max_hp = r.get<double>();
        d.spawn_sector = r.get<int32_t>();
        d.sector = r.get<int32_t>();
        d.wave_dir = r.get<int32_t>();
        d.shards_collected = r.get<int32_t>();
        d.reached_nexus = r.get<uint8_t>() != 0;
        d.alive = r.get<uint8_t>() != 0;
        w.demons_.push_back(d);
    }
    return r.ok;
}

} // namespace mad::core
