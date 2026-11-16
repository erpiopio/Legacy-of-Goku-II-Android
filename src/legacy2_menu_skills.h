#pragma once

#include "legacy2_menu_state.h"
#include <string>

namespace legacy2::menu {
struct Skill {
    std::u16string title;
    std::u16string description;
    int available = -1;
};
using Skills = std::array<Skill, 3>;

inline std::u16string original_text(const std::uint8_t* rom, std::size_t size,
                                  unsigned id, unsigned language) {
    constexpr std::size_t table = 0x6C9044;
    const std::size_t entry = table + id * 20u + language * 4u;
    if (!rom || language >= 5 || entry + 4 > size) return {};
    const auto address = read32(rom + entry);
    if (address < 0x08000000u || address - 0x08000000u >= size) return {};
    std::u16string text;
    std::size_t offset = address - 0x08000000u;
    for (unsigned count = 0; count < 4096 && offset < size; ++count, ++offset) {
        const std::uint8_t byte = rom[offset];
        if (byte == 0) return text;
        // The original European string table is ISO-8859-1, not UTF-8.
        text.push_back(static_cast<char16_t>(byte));
    }
    return {}; // Refuse unterminated/corrupt cartridge text.
}

inline Skills decode_skills(const std::uint8_t* ram, std::size_t ram_size,
        const std::uint8_t* rom, std::size_t rom_size, unsigned character) {
    Skills out;
    if (!ram || ram_size < 0x1214 || character >= kCharacterCount ||
        ram[0x1213] >= 5) return out;
    const auto* record = ram + 0x10B8 + character * 0x1C;
    if ((record[0x14] & 1u) == 0) return out;
    // Native STATUS subpages E294 / E11C only consult these flags/texts;
    // browsing them does not equip attacks or switch the controlled actor.
    constexpr unsigned masks[] = {4, 8, 16};
    constexpr unsigned names[] = {0x9D, 0xA7, 0xB1};
    constexpr unsigned descriptions[] = {0xA2, 0xAC, 0xB6};
    for (unsigned slot = 0; slot < 3; ++slot) {
        // Only five fighters have entries in the native combat-name tables.
        // Mr. Satan uses the original locked text even if an unsupported
        // technique flag is set; never emit an empty or unrelated sixth name.
        const bool unlocked = character < 5 && (record[0x14] & masks[slot]) != 0;
        const unsigned title = unlocked ? names[slot] + character
                                        : (slot == 0 ? 0x98 : 0x9A);
        const unsigned description = unlocked ? descriptions[slot] + character
                                              : (slot == 0 ? 0x99 : 0x9B);
        // This ALFP port presents Spanish text independently of a loaded
        // save's language byte. Browsing never writes that byte or the actor.
        out[slot].title = original_text(rom, rom_size, title, ram[0x1213]);
        out[slot].description = original_text(rom, rom_size, description, ram[0x1213]);
        if (!out[slot].title.empty() && !out[slot].description.empty())
            out[slot].available = unlocked ? 1 : 0;
    }
    return out;
}
} // namespace legacy2::menu
