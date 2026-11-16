// Behavioral checks against the real ALFP helpers and a copied native GBAS.
// No game memory, ROM, save or state file is modified.
#include "../src/legacy2_menu_state.h"
#include "../src/legacy2_menu_skills.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using Bytes = std::vector<std::uint8_t>;
static Bytes load(const char* path) {
    std::ifstream in(path, std::ios::binary);
    assert(in.good());
    return Bytes(std::istreambuf_iterator<char>(in), {});
}
static void put32(Bytes& data, std::size_t at, std::uint32_t value) {
    assert(at + 4 <= data.size());
    for (unsigned i = 0; i < 4; ++i) data[at + i] = value >> (i * 8);
}
int main(int argc, char** argv) {
    using namespace legacy2::menu;
    assert(argc == 3);
    const Bytes rom = load(argv[1]);
    const Bytes state = load(argv[2]);
    assert(state.size() >= 52 && std::string(state.begin(), state.begin() + 4) == "GBAS");
    std::size_t cursor = 52;
    Bytes bus;
    for (unsigned section = 0; section < read32(state.data() + 48); ++section) {
        assert(cursor + 8 <= state.size());
        const std::string tag(state.begin() + cursor, state.begin() + cursor + 4);
        const auto size = read32(state.data() + cursor + 4);
        cursor += 8;
        assert(cursor + size <= state.size());
        if (tag == "BUS0") bus.assign(state.begin() + cursor, state.begin() + cursor + size);
        cursor += size;
    }
    assert(bus.size() >= 0x48000);
    Bytes ewram(bus.begin(), bus.begin() + 0x40000);
    Bytes iwram(bus.begin() + 0x40000, bus.begin() + 0x48000);
    const Bytes original_iwram = iwram;
    const Bytes original_ewram = ewram;
    const Bytes original_rom = rom;
    auto packet = decode_state(iwram.data(), iwram.size(), rom.data(), rom.size(), 7);
    assert(packet[0] == 1 && packet[1] == 4 && packet[2] == 3 && packet[3] == 8 && packet[4] == 7);
    const auto at = kStateHeader + 3 * kCharacterStride;
    assert(packet[at + 2] == 6 && packet[at + 3] == 149 && packet[at + 4] == 149);
    assert(packet[at + 5] == 39 && packet[at + 6] == 39);
    assert(packet[at + 7] == 11 && packet[at + 8] == 9 && packet[at + 9] == 10);
    assert(packet[at + 10] == 1000 && packet[at + 11] == 550);
    for (unsigned id = 0; id < 6; ++id) {
        if (id == 3) continue;
        const auto offset = kStateHeader + id * kCharacterStride;
        assert(packet[offset] == 0 && packet[offset + 1] == static_cast<int>(id));
        for (unsigned field = 2; field < kCharacterStride; ++field) assert(packet[offset + field] == -1);
        for (const auto& skill : decode_skills(iwram.data(), iwram.size(), rom.data(), rom.size(), id))
            assert(skill.available == -1 && skill.title.empty() && skill.description.empty());
    }
    auto skills = decode_skills(iwram.data(), iwram.size(), rom.data(), rom.size(), 3);
    assert(skills[0].title == u"Ataque Especial Cuerpo a Cuerpo");
    assert(skills[1].title == u"Ataque Energético Especial");
    for (const auto& skill : skills)
        assert(skill.available == 0 && skill.description == u"Esta técnica no está disponible todavía");
    auto fixture = iwram;
    put32(fixture, 0x110C + 0x10, read32(rom.data() + 0x2A6A0 + 49 * 4));
    auto capped = decode_state(fixture.data(), fixture.size(), rom.data(), rom.size(), 8);
    assert(capped[at + 2] == 50 && capped[at + 11] == -1);
    fixture = iwram;
    fixture[0x10B8 + 0x14] |= 1;
    fixture[0x110C + 0x14] |= 4 | 8 | 16;
    auto unlocked = decode_state(fixture.data(), fixture.size(), rom.data(), rom.size(), 9);
    assert(unlocked[3] == 9 && unlocked[2] == 3 && fixture[0x10AC] == 3);
    skills = decode_skills(fixture.data(), fixture.size(), rom.data(), rom.size(), 3);
    assert(skills[0].available == 1 && skills[0].title == u"Estocada Cruzada");
    assert(skills[1].available == 1 && skills[1].title == u"Ataque Ardiente");
    assert(skills[2].available == 1 && skills[2].title == u"Descarga de Espada");
    assert(skills[0].description.find(u"Mantén") != std::u16string::npos);
    fixture[0x1144 + 0x14] |= 1 | 4 | 8 | 16;
    for (const auto& skill : decode_skills(fixture.data(), fixture.size(), rom.data(), rom.size(), 5)) {
        assert(skill.available == 0 && !skill.title.empty());
        assert(skill.description == u"Esta técnica no está disponible todavía");
    }
    // Spanish presentation remains Spanish for an older English-language save.
    fixture[0x1213] = 0;
    skills = decode_skills(fixture.data(), fixture.size(), rom.data(), rom.size(), 3);
    assert(skills[0].title == u"Estocada Cruzada");
    assert(skills[1].title == u"Ataque Ardiente");
    fixture[0x1213] = 4;
    auto absent = decode_state(nullptr, 0, rom.data(), rom.size(), 10);
    assert(absent[3] == 0);
    auto effective = packet;
    apply_live_stats(effective, iwram.data(), 0x2000,
        ewram.data(), ewram.size(), rom.data(), rom.size());
    assert(effective[at + 7] == 11 && effective[at + 8] == 9 && effective[at + 9] == 10);
    // Prove the actual actor path is read, rather than only comparing a case
    // where its effective values happen to equal the stored base attributes.
    auto actor_fixture = ewram;
    actor_fixture[0x2FA80 + 0x160] = 73;
    actor_fixture[0x2FA80 + 0x161] = 42;
    actor_fixture[0x2FA80 + 0x162] = 19;
    auto live_override = unlocked;
    apply_live_stats(live_override, fixture.data(), 0x2000,
        actor_fixture.data(), actor_fixture.size(), rom.data(), rom.size());
    assert(live_override[at + 7] == 73 && live_override[at + 8] == 42 && live_override[at + 9] == 19);
    const auto gohan_at = kStateHeader;
    assert(live_override[gohan_at + 7] == unlocked[gohan_at + 7]);
    assert(live_override[gohan_at + 8] == unlocked[gohan_at + 8]);
    assert(live_override[gohan_at + 9] == unlocked[gohan_at + 9]);
    assert(live_override[2] == 3 && fixture[0x10AC] == 3);
    // Unsupported actors and missing memory retain verified stored values.
    put32(actor_fixture, 0x2FA80, 0x08023D18);
    auto unsupported = packet;
    apply_live_stats(unsupported, iwram.data(), 0x2000,
        actor_fixture.data(), actor_fixture.size(), rom.data(), rom.size());
    assert(unsupported == packet);
    apply_live_stats(unsupported, iwram.data(), 0x2000,
        nullptr, 0, rom.data(), rom.size());
    assert(unsupported == packet);
    assert(iwram == original_iwram && ewram == original_ewram && rom == original_rom);
    std::cout << "ALFP menu helper/native-state checks PASS: masks, HP/PE, attributes, EXP/SIG, cap, skills, accents, locked-data suppression, read-only.\n";
}
