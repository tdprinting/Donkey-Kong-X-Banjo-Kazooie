#include "jiggy/save_store.hpp"

#include <algorithm>

namespace jiggy {

namespace {

constexpr uint8_t kMagic[4] = {'J', 'G', 'S', 'V'};
constexpr uint32_t kVersion = 1;
constexpr size_t kHeaderSize = 12;
constexpr size_t kEntrySize = 12;

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

uint32_t read_le32(std::span<const uint8_t> data, size_t offset) {
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) | (uint32_t(data[offset + 2]) << 16) |
           (uint32_t(data[offset + 3]) << 24);
}

void push_le32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        out.push_back(uint8_t(v >> (8 * i)));
    }
}

} // namespace

std::optional<SaveStore> SaveStore::parse(std::span<const uint8_t> data, std::string* error) {
    if (data.size() < kHeaderSize || !std::equal(std::begin(kMagic), std::end(kMagic), data.begin())) {
        set_error(error, "not a Jiggy Jungle save");
        return std::nullopt;
    }
    if (read_le32(data, 4) != kVersion) {
        set_error(error, "unknown Jiggy Jungle save version");
        return std::nullopt;
    }
    const uint64_t count = read_le32(data, 8);
    if (count * kEntrySize != data.size() - kHeaderSize) {
        set_error(error, "Jiggy Jungle save is the wrong size");
        return std::nullopt;
    }
    SaveStore store;
    for (size_t i = 0; i < count; i++) {
        const size_t at = kHeaderSize + i * kEntrySize;
        store.set(read_le32(data, at), read_le32(data, at + 4), read_le32(data, at + 8));
    }
    return store;
}

std::vector<uint8_t> SaveStore::serialize() const {
    std::vector<uint8_t> out(std::begin(kMagic), std::end(kMagic));
    push_le32(out, kVersion);
    push_le32(out, uint32_t(values_.size()));
    for (const auto& [where, value] : values_) {
        push_le32(out, where.first);
        push_le32(out, where.second);
        push_le32(out, value);
    }
    return out;
}

uint32_t SaveStore::get(uint32_t file, uint32_t key) const {
    const auto it = values_.find({file, key});
    return it == values_.end() ? 0 : it->second;
}

bool SaveStore::set(uint32_t file, uint32_t key, uint32_t value) {
    const auto it = values_.find({file, key});
    if (value == 0) {
        if (it == values_.end()) {
            return false;
        }
        values_.erase(it);
        return true;
    }
    if (it != values_.end() && it->second == value) {
        return false;
    }
    values_[{file, key}] = value;
    return true;
}

bool SaveStore::clear_file(uint32_t file) {
    const auto first = values_.lower_bound({file, 0});
    const auto last = values_.upper_bound({file, UINT32_MAX});
    if (first == last) {
        return false;
    }
    values_.erase(first, last);
    return true;
}

} // namespace jiggy
