#pragma once

#include <cstddef>
#include <cstdint>

namespace legacy2::menu {

// ALFP SHA-1 db36ff52fcd63f753f9d66439aa3d2216701c326.
// Native L handler 0801D5AC reads pressed bit 9 at 0300229C+14,
// then calls 08008B04 with 030010B8 + current_character * 1C.
// That routine cycles record[09] modulo four, skipping bits absent from
// record[14] (8 << native_index). It does not select a physical attack.
// Native attack dispatcher 08011FF2 and HUD 0801D734 both obtain the
// type from record[18 + record[09]], then index 0802A76C at stride 10h.
constexpr int kNativeTechniqueCount = 4;
constexpr int kTechniqueTypeCount = 14;
constexpr int kSkillRowCount = 3;
constexpr std::uint32_t kNativeTechniqueCycle = 0x08008B05u;

inline int native_technique_for_skill_row(int row) {
    return row >= 1 && row < kSkillRowCount ? row - 1 : -1;
}

inline int skill_row_for_native_technique(int technique) {
    // STATUS lists the two special energy attacks. Transformation (2) and
    // the basic ki blast (3) stay outside this page and must not highlight
    // either special attack when selected by the physical L button.
    return technique >= 0 && technique < kSkillRowCount - 1
        ? technique + 1 : -1;
}

inline unsigned native_technique_mask(int technique) {
    return technique >= 0 && technique < kNativeTechniqueCount
        ? 8u << technique : 0u;
}

inline const std::uint8_t* technique_record(const std::uint8_t* ram,
        std::size_t size, unsigned character) {
    constexpr std::size_t first = 0x10B8;
    constexpr std::size_t stride = 0x1C;
    if (!ram || character >= 6) return nullptr;
    const auto offset = first + character * stride;
    if (size < offset + stride || (ram[offset + 0x14] & 1u) == 0)
        return nullptr;
    return ram + offset;
}

inline bool technique_unlocked(const std::uint8_t* ram, std::size_t size,
        unsigned character, int technique) {
    const unsigned mask = native_technique_mask(technique);
    const auto* record = technique_record(ram, size, character);
    return record && mask && (record[0x14] & mask) != 0;
}

inline int technique_type(const std::uint8_t* ram, std::size_t size,
        unsigned character, int technique) {
    if (technique < 0 || technique >= kNativeTechniqueCount) return -1;
    const auto* record = technique_record(ram, size, character);
    if (!record) return -1;
    const unsigned type = record[0x18 + technique];
    return type < kTechniqueTypeCount ? static_cast<int>(type) : -1;
}

// A stored byte alone is not confirmation: reject unavailable characters,
// locked techniques and invalid descriptors instead of retaining a stale
// gold highlight. These functions only inspect an immutable publication.
inline int decode_selected_technique(const std::uint8_t* ram,
        std::size_t size, unsigned character) {
    const auto* record = technique_record(ram, size, character);
    if (!record) return -1;
    const int technique = record[0x09];
    if (!technique_unlocked(ram, size, character, technique) ||
        technique_type(ram, size, character, technique) < 0) return -1;
    return technique;
}

} // namespace legacy2::menu
