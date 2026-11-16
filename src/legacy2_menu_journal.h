#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "legacy2_menu_state.h"
#include "legacy2_menu_inventory.h"

namespace legacy2::menu {

constexpr std::size_t kJournalCount = 43;
constexpr std::size_t kJournalTable = 0x6D1298;
constexpr std::size_t kJournalEnd = 0x6D17F8;

struct JournalEntry {
    std::u16string text;
    std::int8_t status = -1; // -1 desconocida, 0 oculta, 1 pendiente, 2 terminada
    std::uint8_t category = 0;
};

using Journal = std::array<JournalEntry, kJournalCount>;

struct JournalValue {
    std::int32_t value = 0;
    bool known = true;
};

inline JournalValue evaluate_journal_condition(
        const std::uint8_t* rom, std::size_t rom_size,
        const std::uint8_t* ram, std::size_t ram_size,
        std::size_t start, std::size_t end,
        const ItemCounts* counts = nullptr) {
    JournalValue stack[16]{};
    unsigned depth = 0;

    if (!rom || !ram || start >= end || end > rom_size)
        return {0, false};

    auto push = [&](JournalValue value) -> bool {
        if (depth >= 16) return false;
        stack[depth++] = value;
        return true;
    };

    for (std::size_t pos = start; pos < end;) {
        const unsigned op = rom[pos++];

        if (op == 0x11)
            return depth == 1 ? stack[0] : JournalValue{0, false};

        if (op == 0x00) {
            if (pos >= end || !push({rom[pos++], true}))
                return {0, false};
        } else if (op == 0x01) {
            std::uint32_t number = 0;
            bool ended = false;
            for (unsigned n = 0; n < 5 && pos < end; ++n) {
                const unsigned byte = rom[pos++];
                number = (number << 7) | (byte & 0x7f);
                if ((byte & 0x80) == 0) {
                    ended = true;
                    break;
                }
            }
            if (!ended) return {0, false};
            const std::int32_t signed_number = (number & 1)
                ? -static_cast<std::int32_t>((number >> 1) + 1)
                : static_cast<std::int32_t>(number >> 1);
            if (!push({signed_number, true}))
                return {0, false};
        } else if (op == 0x02) {
            if (pos >= end || depth == 0) return {0, false};
            const unsigned function = rom[pos++];
            auto& value = stack[depth - 1];

            if (!value.known) continue;

            if (function == 0x13) {
                const std::int32_t id = value.value;
                if (id < 0) return {0, false};
                const std::size_t offset =
                    0x1160u + static_cast<unsigned>(id) / 8u;
                if (offset >= ram_size) return {0, false};
                value.value =
                    (ram[offset] >> (static_cast<unsigned>(id) % 8u)) & 1u;
            } else if (function == 0x15) {
                // Operador original: cantidad del objeto indicado.
                const std::int32_t id = value.value;

                if (!counts || id < 0 ||
                    static_cast<std::size_t>(id) >= counts->size() ||
                    (*counts)[static_cast<std::size_t>(id)] < 0) {
                    value = {0, false};
                } else {
                    value.value =
                        (*counts)[static_cast<std::size_t>(id)];
                }
            } else {
                return {0, false};
            }
        } else if (op == 0x04 || op == 0x05) {
            if (depth < 2) return {0, false};
            const JournalValue right = stack[--depth];
            JournalValue& left = stack[depth - 1];

            const bool left_true = left.known && left.value != 0;
            const bool right_true = right.known && right.value != 0;
            const bool left_false = left.known && left.value == 0;
            const bool right_false = right.known && right.value == 0;

            if (op == 0x04) {
                if (left_false || right_false) left = {0, true};
                else if (left_true && right_true) left = {1, true};
                else left = {0, false};
            } else {
                if (left_true || right_true) left = {1, true};
                else if (left_false && right_false) left = {0, true};
                else left = {0, false};
            }
        } else {
            return {0, false};
        }
    }

    return {0, false};
}

inline Journal decode_journal(
        const std::uint8_t* ram, std::size_t ram_size,
        const std::uint8_t* rom, std::size_t rom_size,
       const ItemCounts* counts = nullptr, unsigned language = 4u) {
    Journal result{};
    if (language >= 5) language = 4u;

    if (!ram || ram_size < 0x2000 || !rom ||
        rom_size < kJournalEnd) return result;

    auto rom_offset = [&](std::uint32_t pointer) -> std::size_t {
        if (pointer < 0x08000000u) return rom_size;
        return static_cast<std::size_t>(pointer - 0x08000000u);
    };

    for (std::size_t id = 0; id < kJournalCount; ++id) {
        const std::size_t record = kJournalTable + id * 32u;
        auto& entry = result[id];
        entry.category = static_cast<std::uint8_t>(read32(rom + record));

        const std::size_t a = rom_offset(read32(rom + record + 4));
        const std::size_t b = rom_offset(read32(rom + record + 8));
        const std::size_t next = id + 1 < kJournalCount
            ? rom_offset(read32(rom + record + 32u + 4u))
            : kJournalTable;

        if (!(a < b && b < next && next <= rom_size))
            continue;

        const std::size_t text_offset =
            rom_offset(read32(rom + record + 12u + language * 4u));

        if (text_offset < rom_size) {
            for (std::size_t p = text_offset;
                 p < rom_size && p - text_offset < 512; ++p) {
                const unsigned char ch = rom[p];
                if (ch == 0) break;
                if (ch == '\r' || ch == '\n')
                    entry.text.push_back(u' ');
                else if (ch >= 32)
                    entry.text.push_back(static_cast<char16_t>(ch));
            }
        }

        const JournalValue completed =
            evaluate_journal_condition(
               rom, rom_size, ram, ram_size, a, b, counts);

        if (completed.known && completed.value != 0) {
            entry.status = 2;
            continue;
        }

        const JournalValue pending =
            evaluate_journal_condition(
               rom, rom_size, ram, ram_size, b, next, counts);

        if (completed.known && pending.known) {
            entry.status = pending.value != 0 ? 1 : 0;
        }
    }

    return result;
}

} // namespace legacy2::menu
