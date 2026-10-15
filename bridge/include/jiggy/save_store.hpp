// What Jiggy Jungle adds to a save file: small values (collected Music Notes, Jinjos and so
// on) kept per DK64 save file, beside DK64's own save data rather than inside it, so DK64's
// save layout stays untouched.
//
// Stored as a little-endian file:
//   "JGSV", u32 version (1), u32 count, then count x { u32 file, u32 key, u32 value }
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace jiggy {

class SaveStore {
public:
    static std::optional<SaveStore> parse(std::span<const uint8_t> data, std::string* error);
    std::vector<uint8_t> serialize() const;

    // 0 for a value never set.
    uint32_t get(uint32_t file, uint32_t key) const;
    // Setting 0 removes the value. Returns whether anything changed.
    bool set(uint32_t file, uint32_t key, uint32_t value);
    // Forgets one save file's values, for when the game starts that file afresh. Returns
    // whether anything changed.
    bool clear_file(uint32_t file);

    size_t size() const { return values_.size(); }

private:
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> values_;
};

} // namespace jiggy
