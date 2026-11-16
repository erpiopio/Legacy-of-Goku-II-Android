#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace gba { class GbaBus; }

namespace legacy2 {

struct MenuPortrait {
    static constexpr unsigned kWidth = 64;
    static constexpr unsigned kHeight = 64;
    int width = 0;
    int height = 0;
    std::array<std::uint32_t, kWidth * kHeight> argb{};
};

// Engine-thread only. Reads the selected character's current ALFP template,
// without changing the controlled character or any guest memory.
// On failure out is cleared, so an earlier portrait can never remain visible.
bool decode_menu_portrait(const gba::GbaBus* bus, unsigned character,
                          MenuPortrait* out);

// Pure-ROM entry for validation. The caller's title identity gate must already
// have verified the imported ALFP dump. No files or graphics assets are created.
bool decode_menu_portrait_template(const std::uint8_t* rom, std::size_t size,
                                   unsigned template_id, MenuPortrait* out);
void reset_menu_portraits();

} // namespace legacy2
