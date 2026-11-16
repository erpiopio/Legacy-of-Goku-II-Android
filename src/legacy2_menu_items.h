#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "legacy2_menu_state.h"

namespace legacy2::menu {

constexpr std::size_t kItemCount = 46;
// Inventory IDs index the 56-byte definitions, not the order of the text
// groups. Native 0800B31A..B330 reads the name ID at definition+4 and
// 0800B284..B29C reads the description ID at +6.
constexpr std::size_t kItemDefinitionTable = 0x696BC8;
constexpr std::size_t kItemDefinitionStride = 56;
constexpr std::size_t kItemTextTable = 0x6C9044;

struct ItemText {
    std::u16string name;
    std::u16string description;
    bool valid = false;
};

using ItemCatalog = std::array<ItemText, kItemCount>;

inline std::u16string read_item_text(
        const std::uint8_t* rom,
        std::size_t rom_size,
        std::uint32_t pointer) {
    std::u16string result;

    if (!rom || pointer < 0x08000000u)
        return result;

    const std::size_t start =
        static_cast<std::size_t>(pointer - 0x08000000u);

    if (start >= rom_size)
        return result;

    for (std::size_t i = start;
         i < rom_size && i - start < 256; ++i) {
        const std::uint8_t ch = rom[i];

        if (ch == 0)
            return result;

        if (ch == '\n' || ch == '\r')
            result.push_back(u' ');
        else if (ch >= 32)
            result.push_back(static_cast<char16_t>(ch));
        else
            return {};
    }

    return {};
}

inline ItemCatalog decode_item_catalog(
        const std::uint8_t* rom,
        std::size_t rom_size,
        unsigned language = 4u) {
    ItemCatalog catalog{};
    if (language >= 5) language = 4;

    if (!rom ||
        rom_size < kItemDefinitionTable +
                   kItemCount * kItemDefinitionStride)
        return catalog;

    for (std::size_t id = 0; id < kItemCount; ++id) {
        const std::size_t record =
            kItemDefinitionTable + id * kItemDefinitionStride;

        // The native ALFP name/description IDs are not in inventory order.
        // Each localized string has five absolute pointers; Spanish is +16.
        const std::size_t name_entry = kItemTextTable +
            read16(rom + record + 4) * 20u + language * 4u;
        const std::size_t description_entry = kItemTextTable +
            read16(rom + record + 6) * 20u + language * 4u;
        if (name_entry + 4 > rom_size || description_entry + 4 > rom_size)
            continue;
        const std::uint32_t name_pointer = read32(rom + name_entry);
        const std::uint32_t description_pointer = read32(rom + description_entry);

        auto& item = catalog[id];

        item.name = read_item_text(
            rom, rom_size, name_pointer);

        item.description = read_item_text(
            rom, rom_size, description_pointer);

        item.valid = !item.name.empty() &&
                     !item.description.empty();
    }

    return catalog;
}

} // namespace legacy2::menu
