// Host-JVM contract test. Includes the actual production JNI implementation;
// only the emulator publication is synthetic. No guest execution/save writes.
#include "../src/legacy2_menu_bridge.cpp"
#include <vector>

namespace gbarecomp { gba::GbaBus* active_bus() { return nullptr; } }
namespace legacy2 {
bool decode_menu_portrait(const gba::GbaBus*, unsigned, MenuPortrait*) {
    return false;
}
}

extern "C" JNIEXPORT void JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuJniTest_publish(
        JNIEnv*, jclass, jint generation, jint selected, jboolean available) {
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    g_state.fill(-1);
    g_state[0] = 1;
    g_state[1] = 4;
    g_state[2] = available ? selected : -1;
    g_state[3] = available ? 1 << selected : 0;
    g_state[4] = generation;
    g_probe_ram.fill(0);
    g_probe_ram[0x10AC] = available ? selected : 0xFF;
    g_probe_ram[0x1213] = 4;
    for (int id = 0; id < 6; ++id) {
        const int offset = 5 + id * 12;
        g_state[offset] = available && id == selected ? 1 : 0;
        g_state[offset + 1] = id;
        g_state[offset + 3] = generation;
        g_portraits[id].width = g_portraits[id].height = 1;
        g_portraits[id].argb[0] = generation;
        auto* record = g_probe_ram.data() + 0x10B8 + id * 0x1C;
        record[0x14] = available && id == selected ? 0x19 : 0;
        record[0x09] = generation % 2;
        record[0x18] = generation % legacy2::menu::kTechniqueTypeCount;
        record[0x19] = (generation + 1) % legacy2::menu::kTechniqueTypeCount;
        g_skill_icon_ids[id] = {{-1,
            legacy2::menu::technique_type(g_probe_ram.data(), g_probe_ram.size(), id, 0),
            legacy2::menu::technique_type(g_probe_ram.data(), g_probe_ram.size(), id, 1)}};
    }
    const int native = available ? legacy2::menu::decode_selected_technique(
        g_probe_ram.data(), g_probe_ram.size(), static_cast<unsigned>(selected)) : -1;
    g_skill_selection = {{g_state[2],
        legacy2::menu::skill_row_for_native_technique(native), 0, generation}};
    g_player_actor_present = false; // There is no mutable emulator bus in this fixture.
    g_skill_request = {};
    const std::string digits = std::to_string(generation);
    const std::u16string text(digits.begin(), digits.end());
    for (auto& character : g_skills)
        for (auto& skill : character) {
            skill.title = skill.description = text;
            skill.available = 1;
        }
    for (auto& item : g_items) item.name = item.description = text;
    g_item_counts.fill(generation);
    for (auto& quest : g_journal) {
        quest.text = text;
        quest.status = generation % 3;
        quest.category = 0;
    }
    g_probe_ready = true;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuJniTest_read(
        JNIEnv* env, jclass type, jint selected) {
    return Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_getMenuSnapshot(
        env, type, selected);
}

extern "C" JNIEXPORT void JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuJniTest_publishCopiedState(
        JNIEnv* env, jclass, jbyteArray rom_bytes, jbyteArray bus_bytes) {
    const jsize rom_size = env->GetArrayLength(rom_bytes);
    const jsize memory_size = env->GetArrayLength(bus_bytes);
    if (rom_size != 0x800000 || memory_size < 0x48000) return;
    std::vector<std::uint8_t> rom(rom_size), memory(memory_size);
    env->GetByteArrayRegion(rom_bytes, 0, rom_size,
        reinterpret_cast<jbyte*>(rom.data()));
    if (env->ExceptionCheck()) return;
    env->GetByteArrayRegion(bus_bytes, 0, memory_size,
        reinterpret_cast<jbyte*>(memory.data()));
    if (env->ExceptionCheck()) return;
    const auto* ewram = memory.data();
    const auto* iwram = memory.data() + 0x40000;
    auto state = legacy2::menu::decode_state(iwram, 0x8000,
        rom.data(), rom.size(), 5000);
    legacy2::menu::apply_live_stats(state, iwram, 0x8000, ewram, 0x40000,
        rom.data(), rom.size());
    std::array<legacy2::menu::Skills, 6> skills{};
    std::array<std::array<std::int32_t, 3>, 6> skill_icon_ids{};
    for (auto& icons : skill_icon_ids) icons.fill(-1);
    for (unsigned id = 0; id < 6; ++id)
        if (state[3] & (1 << id)) {
            const auto* record = legacy2::menu::technique_record(iwram, 0x8000, id);
            if (record && record[8] < 22)
                skill_icon_ids[id][0] = -1 - static_cast<int>(record[8]);
            skills[id] = legacy2::menu::decode_skills(iwram, 0x8000,
                rom.data(), rom.size(), id);
            for (unsigned slot = 1; slot < 3; ++slot)
                skill_icon_ids[id][slot] = legacy2::menu::technique_type(
                    iwram, 0x8000, id, slot - 1);
        }
    const int hero = state[2];
    const int native = hero >= 0 && hero < 6
        ? legacy2::menu::decode_selected_technique(iwram, 0x8000,
            static_cast<unsigned>(hero)) : -1;
    const std::array<std::int32_t, 4> skill_selection{{hero,
        legacy2::menu::skill_row_for_native_technique(native), 0, state[4]}};
    auto items = legacy2::menu::decode_item_catalog(rom.data(), rom.size());
    auto counts = legacy2::menu::decode_item_counts(iwram, 0x8000,
        ewram, 0x40000, rom.data(), rom.size());
    auto journal = legacy2::menu::decode_journal(iwram, 0x8000,
        rom.data(), rom.size(), &counts);
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    g_state = state;
    std::memcpy(g_probe_ram.data(), iwram, g_probe_ram.size());
    g_portraits = {};
    g_skills = std::move(skills);
    g_skill_icon_ids = skill_icon_ids;
    g_skill_selection = skill_selection;
    g_player_actor_present = false;
    g_skill_request = {};
    g_items = std::move(items);
    g_item_counts = counts;
    g_journal = std::move(journal);
    g_probe_ready = true;
}
