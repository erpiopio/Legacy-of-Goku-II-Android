#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "legacy2_menu_items.h"

namespace legacy2::menu {

using ItemCounts = std::array<int, kItemCount>;

inline ItemCounts decode_item_counts(
        const std::uint8_t* iwram, std::size_t iwram_size,
        const std::uint8_t* ewram, std::size_t ewram_size,
        const std::uint8_t* rom, std::size_t rom_size) {

    ItemCounts counts{};
    counts.fill(-1);

    if (!iwram || iwram_size < 0x2000 ||
        !ewram || !rom) return counts;

    constexpr std::size_t stateCounters = 0x11D7;
    constexpr std::size_t objectManager = 0x13B0;
    constexpr std::uint32_t inventoryHandler = 0x080161A1u;

    if (rom_size < 0x25800) return counts;

    for (std::size_t id = 0; id < kItemCount; ++id)
        counts[id] = iwram[stateCounters + id];

    auto memory_at = [&](std::uint32_t address,
                         std::size_t bytes) -> const std::uint8_t* {
        if (address >= 0x02000000u &&
            address < 0x02040000u) {
            const std::size_t off = address - 0x02000000u;
            if (off <= ewram_size &&
                bytes <= ewram_size - off)
                return ewram + off;
        }

        if (address >= 0x03000000u &&
            address < 0x03008000u) {
            const std::size_t off = address - 0x03000000u;
            if (off <= iwram_size &&
                bytes <= iwram_size - off)
                return iwram + off;
        }

        return nullptr;
    };

    for (unsigned collection = 0; collection < 2; ++collection) {
        // Segunda colección: desplazamiento 0x40C,
        // confirmado por LDR literal en ROM 0x1E6AC.
        // The second count is at +0x40C, not +0x410. Native
        // 0801E67E reads the first count at manager+4; 0801E6AC..B4
        // advances directly to manager+0x40C and reads its count there.
        const std::size_t count_offset = objectManager +
            (collection == 0 ? 4u : 0x40Cu);
        const std::size_t entries_offset = count_offset + 4u;

        if (count_offset + 4 > iwram_size ||
            entries_offset > iwram_size) {
            counts.fill(-1);
            return counts;
        }

        const std::uint32_t amount = read32(iwram + count_offset);

        if (amount > 256) {
            counts.fill(-1);
            return counts;
        }

        for (std::uint32_t index = 0; index < amount; ++index) {
            const std::size_t slot = entries_offset + index * 4u;
            if (slot + 4 > iwram_size) {
                counts.fill(-1);
                return counts;
            }

            const std::uint32_t address = read32(iwram + slot);
            const auto* actor = memory_at(address, 0x44);
            if (!actor) continue;

            const std::uint32_t vtable = read32(actor);
            if (vtable < 0x08000000u) continue;

            const std::size_t vt =
                static_cast<std::size_t>(vtable - 0x08000000u);

            if (vt > rom_size || 0x28 > rom_size - vt)
                continue;

            const std::uint32_t relative =
                read32(rom + vt + 0x24);

            // Puntero relativo a la base de la tabla virtual.
            const std::uint32_t handler = vtable + relative;

            if (handler != inventoryHandler)
                continue;

            const std::uint32_t id = read32(actor + 0x40);

            if (id < kItemCount)
                ++counts[id];
        }
    }

    return counts;
}

} // namespace legacy2::menu
