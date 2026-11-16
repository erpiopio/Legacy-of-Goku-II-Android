#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace legacy2::menu {

constexpr std::size_t kCharacterCount = 6;
constexpr std::size_t kStateHeader = 5;
constexpr std::size_t kCharacterStride = 12;
using StatePacket = std::array<std::int32_t,
    kStateHeader + kCharacterCount * kCharacterStride>;

inline std::uint16_t read16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
inline std::uint32_t read32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(read16(p)) |
        (static_cast<std::uint32_t>(read16(p + 2)) << 16);
}

// Verified against ALFP 08008390, 08008DA4, 08008A94, 08008AB0 and
// the original STATUS renderer 0800E3CC. This reads immutable copies only.
inline StatePacket decode_state(const std::uint8_t* ram, std::size_t ram_size,
        const std::uint8_t* rom, std::size_t rom_size, std::uint32_t generation) {
    StatePacket out;
    out.fill(-1);
    out[0] = 1;
    out[3] = 0;
    out[4] = static_cast<std::int32_t>(generation & 0x7fffffffu);
    for (std::size_t id = 0; id < kCharacterCount; ++id) {
        out[kStateHeader + id * kCharacterStride] = 0;
        out[kStateHeader + id * kCharacterStride + 1] = static_cast<int>(id);
    }
    constexpr std::size_t kFirstRecord = 0x10B8;
    constexpr std::size_t kRecordBytes = 0x1C;
    constexpr std::size_t kExperienceTable = 0x2A6A0;
    if (!ram || ram_size < 0x1214 ||
        !rom || rom_size < kExperienceTable + 50 * 4)
        return out;
    out[1] = ram[0x1213] <= 4 ? ram[0x1213] : -1;
    out[2] = ram[0x10AC] < kCharacterCount ? ram[0x10AC] : -1;
    for (std::size_t id = 0; id < kCharacterCount; ++id) {
        const auto* record = ram + kFirstRecord + id * kRecordBytes;
        // Initialized statistics do not imply availability. The original
        // party counter tests bit zero of this exact flag byte.
        if ((record[0x14] & 1u) == 0) continue;
        out[3] |= 1 << id;
        auto* value = out.data() + kStateHeader + id * kCharacterStride;
        value[0] = 1;
        const std::uint32_t exp = read32(record + 0x10);
        unsigned level = 1;
        for (int i = 49; i >= 0; --i) {
            if (exp >= read32(rom + kExperienceTable + i * 4)) {
                level = static_cast<unsigned>(i + 1);
                break;
            }
        }
        value[2] = static_cast<int>(level);
        value[3] = read16(record);
        value[4] = read16(record + 2);
        value[5] = read16(record + 4);
        value[6] = read16(record + 6);
        value[7] = read16(record + 0x0A) >> 8;
        value[8] = read16(record + 0x0C) >> 8;
        value[9] = read16(record + 0x0E) >> 8;
        if (exp <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
            value[10] = static_cast<std::int32_t>(exp);
        // The native menu omits SIG at the level-50 cap.
        if (level < 50)
            value[11] = static_cast<std::int32_t>(
                read32(rom + kExperienceTable + level * 4) - exp);
    }
    return out;
}

// The native STATUS page uses computed actor attributes for the controlled
// character, and the stored attributes for other party members. Query event 7
// returns the player object; identify that exact handler without executing
// guest code or changing the current character (ALFP E580 / 1E61A / 15A30).
inline bool apply_live_stats(StatePacket& packet,
        const std::uint8_t* iwram, std::size_t iwram_size,
        const std::uint8_t* ewram, std::size_t ewram_size,
        const std::uint8_t* rom, std::size_t rom_size) {
    const int active = packet[2];
    if (!iwram || iwram_size < 0x1FAC || !ewram || !rom || active < 0 ||
        active >= 6 || !(packet[3] & (1 << active))) return false;
    auto ram_at = [&](std::uint32_t address, std::size_t count)
            -> const std::uint8_t* {
        if (address < 0x02000000u) return nullptr;
        const std::size_t offset = address - 0x02000000u;
        if (offset > ewram_size || count > ewram_size - offset) return nullptr;
        return ewram + offset;
    };
    const auto query = read32(iwram + 0x1FA8);
    for (unsigned list = 0; list < 2; ++list) {
        const unsigned count_offset = list ? 0x40C : 4;
        const unsigned items_offset = list ? 0x410 : 8;
        const auto* count_data = ram_at(query + count_offset, 4);
        if (!count_data) continue;
        const auto count = read32(count_data);
        if (count > 256) continue;
        const auto* items = ram_at(query + items_offset, count * 4u);
        if (!items) continue;
        for (unsigned i = count; i > 0; --i) {
            const auto* actor = ram_at(read32(items + (i - 1) * 4), 0x163);
            if (!actor) continue;
            const auto vtable = read32(actor);
            if (vtable < 0x08000000u) continue;
            const std::size_t offset = vtable - 0x08000000u;
            if (offset > rom_size || 0x28 > rom_size - offset) continue;
            const auto handler = vtable + read32(rom + offset + 0x24);
            if (handler != 0x08015A31u) continue;
            auto* record = packet.data() + kStateHeader + active * kCharacterStride;
            record[7] = actor[0x160];
            record[8] = actor[0x161];
            record[9] = actor[0x162];
            return true;
        }
    }
    return false;
}

} // namespace legacy2::menu
