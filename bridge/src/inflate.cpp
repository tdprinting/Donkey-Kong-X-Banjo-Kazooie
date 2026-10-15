#include "jiggy/inflate.hpp"

#include <algorithm>

#include "byte_io.hpp"
#include "miniz.h"

namespace jiggy {

namespace {

void set_error(std::string* error, const char* message) {
    if (error) {
        *error = message;
    }
}

// Inflates raw DEFLATE, reporting how many input bytes the stream used so callers
// can find a trailer after it.
std::optional<std::vector<uint8_t>> inflate_raw_impl(std::span<const uint8_t> data, size_t size_hint,
                                                     size_t* consumed, std::string* error) {
    // Rare's assets are small; the cap only guards against a corrupt stream eating memory.
    constexpr size_t kMaxOutput = 64 * 1024 * 1024;

    mz_stream stream{};
    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) {
        set_error(error, "failed to initialise inflate");
        return std::nullopt;
    }

    std::vector<uint8_t> out(size_hint ? size_hint : std::max<size_t>(data.size() * 4, 256));
    stream.next_in = data.data();
    stream.avail_in = static_cast<unsigned int>(data.size());

    int status = MZ_OK;
    while (true) {
        stream.next_out = out.data() + stream.total_out;
        stream.avail_out = static_cast<unsigned int>(out.size() - stream.total_out);
        status = mz_inflate(&stream, MZ_NO_FLUSH);
        if (status == MZ_STREAM_END) {
            break;
        }
        if (status == MZ_BUF_ERROR && stream.avail_out == 0) {
            if (out.size() >= kMaxOutput) {
                mz_inflateEnd(&stream);
                set_error(error, "inflated data exceeds the 64 MiB safety limit");
                return std::nullopt;
            }
            out.resize(std::min(out.size() * 2, kMaxOutput));
            continue;
        }
        if (status != MZ_OK) {
            mz_inflateEnd(&stream);
            set_error(error, "corrupt DEFLATE stream");
            return std::nullopt;
        }
        if (stream.avail_in == 0) {
            mz_inflateEnd(&stream);
            set_error(error, "DEFLATE stream is truncated");
            return std::nullopt;
        }
        if (stream.avail_out == 0) {
            if (out.size() >= kMaxOutput) {
                mz_inflateEnd(&stream);
                set_error(error, "inflated data exceeds the 64 MiB safety limit");
                return std::nullopt;
            }
            out.resize(std::min(out.size() * 2, kMaxOutput));
        }
    }

    out.resize(stream.total_out);
    if (consumed) {
        *consumed = stream.total_in;
    }
    mz_inflateEnd(&stream);
    return out;
}

} // namespace

bool is_rarezip(std::span<const uint8_t> data) {
    return data.size() >= kRarezipHeaderSize && data[0] == kRarezipMagic0 && data[1] == kRarezipMagic1;
}

bool is_gzip(std::span<const uint8_t> data) {
    return data.size() >= 18 && data[0] == 0x1F && data[1] == 0x8B && data[2] == 0x08;
}

std::optional<uint32_t> rarezip_uncompressed_size(std::span<const uint8_t> data) {
    if (!is_rarezip(data)) {
        return std::nullopt;
    }
    return read_be32(data, 2);
}

std::optional<std::vector<uint8_t>> inflate_raw(std::span<const uint8_t> data, std::string* error) {
    return inflate_raw_impl(data, 0, nullptr, error);
}

std::optional<std::vector<uint8_t>> inflate_rarezip(std::span<const uint8_t> data, std::string* error) {
    auto expected = rarezip_uncompressed_size(data);
    if (!expected) {
        set_error(error, "missing rarezip header (11 72)");
        return std::nullopt;
    }
    auto out = inflate_raw_impl(data.subspan(kRarezipHeaderSize), *expected, nullptr, error);
    if (out && out->size() != *expected) {
        set_error(error, "rarezip size header does not match the inflated data");
        return std::nullopt;
    }
    return out;
}

std::optional<std::vector<uint8_t>> inflate_gzip(std::span<const uint8_t> data, std::string* error) {
    if (!is_gzip(data)) {
        set_error(error, "missing gzip header (1f 8b 08)");
        return std::nullopt;
    }

    constexpr uint8_t kFlagHcrc = 0x02;
    constexpr uint8_t kFlagExtra = 0x04;
    constexpr uint8_t kFlagName = 0x08;
    constexpr uint8_t kFlagComment = 0x10;

    const uint8_t flags = data[3];
    size_t pos = 10;
    if (flags & kFlagExtra) {
        if (pos + 2 > data.size()) {
            set_error(error, "truncated gzip header");
            return std::nullopt;
        }
        pos += 2 + (data[pos] | (data[pos + 1] << 8));
    }
    for (uint8_t string_flag : {kFlagName, kFlagComment}) {
        if (flags & string_flag) {
            while (pos < data.size() && data[pos] != 0) {
                pos++;
            }
            pos++;
        }
    }
    if (flags & kFlagHcrc) {
        pos += 2;
    }
    if (pos >= data.size()) {
        set_error(error, "truncated gzip header");
        return std::nullopt;
    }

    size_t consumed = 0;
    auto out = inflate_raw_impl(data.subspan(pos), 0, &consumed, error);
    if (!out) {
        return std::nullopt;
    }

    const size_t trailer = pos + consumed;
    if (trailer + 8 > data.size()) {
        set_error(error, "gzip trailer is missing");
        return std::nullopt;
    }
    const uint32_t expected_crc = read_le32(data, trailer);
    const uint32_t expected_size = read_le32(data, trailer + 4);
    const uint32_t crc = static_cast<uint32_t>(mz_crc32(MZ_CRC32_INIT, out->data(), out->size()));
    if (crc != expected_crc || static_cast<uint32_t>(out->size()) != expected_size) {
        set_error(error, "gzip CRC or size check failed");
        return std::nullopt;
    }
    return out;
}

} // namespace jiggy
