#include "legacy2_menu_portraits.h"
#include "gba_bus.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace legacy2 {
namespace {

// Verified against the original ALFP STATUS renderer, not another title:
// 0800E728..736: record+8 -> template table (20 bytes), +3 resource ID.
// 0801F39A..A0: resource ID >= 8 -> table+4+id*4 -> descriptor.
// 0800E3FC..402 and 0800E73A..48: decode a 64x64 linear 8-bpp bitmap.
// 0800C2E0..F2 / 0800DE02..14: copy this 256-color palette to BG PAL.
constexpr std::size_t kTemplateTable = 0x2A4E8;
constexpr std::size_t kResourceTable = 0x6A97F8;
constexpr std::size_t kPalette = 0x6A85F8;
constexpr unsigned kTemplateCount = 22;
constexpr std::size_t kPixels = MenuPortrait::kWidth * MenuPortrait::kHeight;

const std::uint8_t* cached_rom = nullptr;
std::size_t cached_size = 0;
std::unordered_map<std::uint32_t, MenuPortrait> decoded;

std::uint16_t u16(const std::uint8_t* data) {
    return std::uint16_t(data[0]) | (std::uint16_t(data[1]) << 8);
}
std::uint32_t u32(const std::uint8_t* data) {
    return std::uint32_t(u16(data)) | (std::uint32_t(u16(data + 2)) << 16);
}
bool range(std::size_t size, std::size_t offset, std::size_t count) {
    return offset <= size && count <= size - offset;
}

class Bits {
public:
    Bits(const std::uint8_t* rom, std::size_t size, std::size_t offset)
        : rom_(rom), size_(size), offset_(offset) {}
    std::uint32_t read(unsigned count) {
        if (count > 31) { good_ = false; return 0; }
        std::uint32_t result = 0;
        while (count--) {
            if (!remaining_) {
                if (!range(size_, offset_, 4)) { good_ = false; return 0; }
                word_ = u32(rom_ + offset_);
                offset_ += 4;
                remaining_ = 32;
            }
            result = (result << 1) | (word_ >> 31);
            word_ <<= 1;
            --remaining_;
        }
        return result;
    }
    std::uint32_t gamma() {
        std::uint32_t value = 1;
        for (unsigned i = 0; i < 30; ++i) {
            value = (value << 1) | read(1);
            if (!read(1) || !good_) return value;
        }
        good_ = false;
        return 0;
    }
    bool good() const { return good_; }
private:
    const std::uint8_t* rom_;
    std::size_t size_, offset_;
    unsigned remaining_ = 0;
    std::uint32_t word_ = 0;
    bool good_ = true;
};

bool decode(const std::uint8_t* rom, std::size_t size, std::uint32_t address,
            std::array<std::uint8_t, kPixels>* out) {
    if (address < 0x08000000u) return false;
    const std::size_t offset = address - 0x08000000u;
    if (!range(size, offset, 8) || u32(rom + offset + 4) != kPixels)
        return false;
    const unsigned kind = u32(rom + offset);
    if (!kind) {
        if (!range(size, offset + 8, kPixels)) return false;
        std::copy_n(rom + offset + 8, kPixels, out->begin());
        return true;
    }
    if (kind != 1 && kind != 2) return false;
    Bits bits(rom, size, offset + 8);
    std::size_t produced = 0;
    unsigned distance_bits = 8, literal_bits = kind;
    std::uint32_t literal_base = 0, last_distance = 1;
    auto append = [&](std::uint32_t value) {
        if (produced == out->size()) return false;
        (*out)[produced++] = static_cast<std::uint8_t>(value);
        return true;
    };
    auto copy = [&](std::uint32_t distance, std::uint32_t count) {
        if (!distance || distance > produced || count > out->size() - produced)
            return false;
        while (count--) {
            (*out)[produced] = (*out)[produced - distance];
            ++produced;
        }
        return true;
    };
    // Same descriptor stream dispatched by ALFP 0801F844 -> 08023B58.
    for (unsigned command = 0; command < 65536 && bits.good(); ++command) {
        if (bits.read(1)) {
            if (literal_bits > 24 || !append(bits.read(literal_bits) + literal_base))
                return false;
        } else if (bits.read(1)) {
            const auto code = bits.gamma();
            std::uint32_t distance, count;
            if (code == 2) {
                distance = last_distance;
                count = bits.gamma();
            } else {
                if (code < 3 || distance_bits > 24) return false;
                distance = bits.read(distance_bits) + ((code - 3) << distance_bits);
                last_distance = distance;
                count = bits.gamma();
                count += distance >= 0x10000 ? 3 : distance >= 0x37FF ? 2 :
                         distance >= 0x027F ? 1 : distance <= 0x7F ? 4 : 0;
            }
            if (!copy(distance, count)) return false;
        } else if (!bits.read(1)) {
            const auto distance = bits.read(7);
            if (distance) {
                last_distance = distance;
                if (!copy(distance, bits.read(2) + 2)) return false;
            } else {
                const auto code = bits.read(2);
                if (!code) return bits.good() && produced == out->size();
                distance_bits = bits.read(code + 3);
            }
        } else {
            const int short_code = static_cast<int>(bits.read(4)) - 1;
            if (!short_code) {
                if (!append(0)) return false;
            } else if (short_code > 0) {
                if (!copy(static_cast<unsigned>(short_code), 1)) return false;
            } else if (bits.read(1)) {
                do {
                    for (unsigned i = 0; i < 256; ++i)
                        if (!append(bits.read(8))) return false;
                } while (bits.read(1) && bits.good());
            } else {
                literal_base = 0;
                literal_bits = 7 + bits.read(1);
                if (literal_bits == 7) literal_base = bits.read(8);
            }
        }
    }
    return false;
}

std::uint32_t color(std::uint16_t bgr) {
    const auto expand = [](unsigned n) { return (n << 3) | (n >> 2); };
    return 0xFF000000u | (expand(bgr & 31) << 16) |
        (expand((bgr >> 5) & 31) << 8) | expand((bgr >> 10) & 31);
}

} // namespace

void reset_menu_portraits() {
    cached_rom = nullptr;
    cached_size = 0;
    decoded.clear();
}

bool decode_menu_portrait_template(const std::uint8_t* rom, std::size_t size,
                                   unsigned template_id, MenuPortrait* out) {
    if (!out) return false;
    *out = MenuPortrait{};
    if (!rom || size != 0x800000 || template_id >= kTemplateCount ||
        std::memcmp(rom + 0xAC, "ALFP", 4) != 0 ||
        !range(size, kTemplateTable, kTemplateCount * 20) ||
        !range(size, kPalette, 512)) return false;
    if (cached_rom != rom || cached_size != size) {
        reset_menu_portraits();
        cached_rom = rom;
        cached_size = size;
    }
    const unsigned resource = rom[kTemplateTable + template_id * 20 + 3];
    // Every portrait ID in the verified 22-entry ALFP template table is 8..25.
    // IDs 1..7 are dynamic resource selectors in the guest and aren't indices.
    if (resource < 8 || resource > 25) return false;
    const auto descriptor = u32(rom + kResourceTable + 4 + resource * 4);
    const auto previous = decoded.find(descriptor);
    if (previous != decoded.end()) { *out = previous->second; return true; }
    std::array<std::uint8_t, kPixels> indices{};
    if (!decode(rom, size, descriptor, &indices)) return false;
    MenuPortrait portrait;
    portrait.width = MenuPortrait::kWidth;
    portrait.height = MenuPortrait::kHeight;
    for (std::size_t i = 0; i < indices.size(); ++i)
        portrait.argb[i] = color(u16(rom + kPalette + indices[i] * 2));
    const auto entry = decoded.emplace(descriptor, std::move(portrait));
    *out = entry.first->second;
    return true;
}

bool decode_menu_portrait(const gba::GbaBus* bus, unsigned character,
                          MenuPortrait* out) {
    if (!out) return false;
    *out = MenuPortrait{};
    if (!bus || character >= 6) return false;
    const auto* record = bus->iwram_ptr() + 0x10B8 + character * 0x1C;
    if (!(u32(record + 0x14) & 1u)) return false;
    return decode_menu_portrait_template(bus->rom_ptr(), bus->rom_size(), record[8], out);
}

} // namespace legacy2
