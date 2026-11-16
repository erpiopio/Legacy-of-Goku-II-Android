#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

#include "legacy2_menu_state.h"
#include "legacy2_menu_skills.h"
#include "legacy2_menu_skill_selection.h"
#include "legacy2_menu_inventory.h"
#include "legacy2_menu_journal.h"
#include "legacy2_menu_portraits.h"
#include "legacy2_scouter_state.h"
#include "gba_bus.h"
#include "runtime_bus_bridge.h"

#if defined(__ANDROID__)
#include <jni.h>
#endif

namespace {
std::atomic<int> g_alfp_language{-1};
std::atomic<int> g_legacy2_item_request_pending{-1};
std::mutex g_probe_mutex;
std::array<std::uint8_t, 0x2000> g_probe_ram{};
legacy2::menu::StatePacket g_state{};
std::array<legacy2::MenuPortrait, 6> g_portraits{};
std::array<legacy2::menu::Skills, 6> g_skills{};
legacy2::menu::ItemCatalog g_items{};
legacy2::menu::ItemCounts g_item_counts{};
legacy2::menu::Journal g_journal{};
std::uint32_t g_generation = 0;
legacy2::scouter::Snapshot g_scouter{};
bool g_probe_ready = false;
bool g_player_actor_present = false;
std::array<std::int32_t, 4> g_skill_selection{{-1, -1, 0, 0}};
std::array<std::array<std::int32_t, 3>, 6> g_skill_icon_ids{};
struct SkillRequest { int hero = -1, slot = -1; std::uint32_t generation = 0; };
SkillRequest g_skill_request{};
}

extern "C" void legacy2_menu_publish_language(int language) {
    g_alfp_language.store(language >= 0 && language <= 4 ? language : -1,
                          std::memory_order_release);
}

extern "C" void legacy2_menu_publish_probe(const std::uint8_t* iwram) {
    if (!iwram) return;
    // Called exclusively by the emulation frame hook. Java never touches
    // mutable guest memory or a GbaBus; all JNI methods copy this publication.
    std::array<std::uint8_t, 0x2000> ram{};
    std::memcpy(ram.data(), iwram, ram.size());
    auto* bus = gbarecomp::active_bus();
    auto state = legacy2::menu::decode_state(ram.data(), ram.size(),
        bus ? bus->rom_ptr() : nullptr, bus ? bus->rom_size() : 0,
        ++g_generation);
    const bool player_actor_present = bus &&
        legacy2::menu::apply_live_stats(state, ram.data(), ram.size(),
            bus->ewram_ptr(), 0x40000,
            bus->rom_ptr(), bus->rom_size());
    std::array<legacy2::MenuPortrait, 6> portraits{};
    std::array<legacy2::menu::Skills, 6> skills{};
    for (unsigned id = 0; bus && id < 6; ++id) {
        if ((state[3] & (1 << id)) == 0) continue;
        legacy2::decode_menu_portrait(bus, id, &portraits[id]);
        skills[id] = legacy2::menu::decode_skills(ram.data(), ram.size(),
            bus->rom_ptr(), bus->rom_size(), id);
    }
    legacy2::menu::ItemCatalog items{};
    legacy2::menu::ItemCounts counts{};
    counts.fill(-1);

    if (bus) {
        items = legacy2::menu::decode_item_catalog(
            bus->rom_ptr(), bus->rom_size(),
            state[1] >= 0 && state[1] <= 4
                ? static_cast<unsigned>(state[1]) : 4u);
        counts = legacy2::menu::decode_item_counts(
            ram.data(), ram.size(),
            bus->ewram_ptr(), 0x40000,
            bus->rom_ptr(), bus->rom_size());
    }

    // Evaluar las misiones usando el mismo snapshot
    // y las cantidades calculadas para OBJETOS.
    legacy2::menu::Journal journal{};
    if (bus) {
        journal = legacy2::menu::decode_journal(
            ram.data(), ram.size(),
            bus->rom_ptr(), bus->rom_size(),
            &counts,
            state[1] >= 0 && state[1] <= 4
                ? static_cast<unsigned>(state[1]) : 4u);
    }

    auto scouter = bus ? legacy2::scouter::decode(
        ram.data(), ram.size(), bus->ewram_ptr(), 0x40000,
        bus->rom_ptr(), bus->rom_size(), g_generation)
        : legacy2::scouter::Snapshot{};
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    g_scouter = scouter;
    g_probe_ram = ram;
    g_state = state;
    g_portraits = std::move(portraits);
    g_skills = std::move(skills);
    g_items = std::move(items);
    g_item_counts = counts;
    g_journal = std::move(journal);
    // ALFP: el actor de STATUS puede no estar en la lista mientras
    // caminamos. Detectar también el escenario real por sus capas BG.
    bool escenarioActivo = false;
    if (bus) {
        const std::uint16_t dispcnt = bus->io().read16(0x000u);

        if ((dispcnt & 0x00C7u) == 0x0040u &&
            (dispcnt & 0x0F00u) != 0u) {
            escenarioActivo = true;

            for (int bg = 0; bg < 4; ++bg) {
                if ((dispcnt & (0x0100u << bg)) == 0u)
                    continue;

                const std::uint16_t cnt =
                    bus->io().read16(0x008u + bg * 2u);

                if (((cnt >> 8) & 0x1Fu) !=
                        static_cast<unsigned>(28 + bg) ||
                    ((cnt >> 14) & 3u) != 0u) {
                    escenarioActivo = false;
                    break;
                }
            }
        }
    }

    g_player_actor_present =
        player_actor_present || escenarioActivo;
    const int hero = state[2];
    int selected_row = -1;
    if (hero >= 0 && hero < 6) {
        const int native = legacy2::menu::decode_selected_technique(
            ram.data(), ram.size(), static_cast<unsigned>(hero));
        // STATUS lists the physical special and two energy specials. The
        // other L positions (transformation/basic Ki) are not those rows.
        if (native == 0 || native == 1) selected_row = native + 1;
    }
    const bool can_select = escenarioActivo && hero >= 0 && hero < 5 &&
        (state[3] & (1 << hero)) &&
        legacy2::menu::read16(ram.data() + 0x10B8 + hero * 0x1C) != 0;
    g_skill_selection = {{hero, selected_row, can_select ? 1 : 0, state[4]}};
    for (unsigned id = 0; id < 6; ++id) {
        g_skill_icon_ids[id].fill(-1);
        const auto* record = legacy2::menu::technique_record(ram.data(), ram.size(), id);
        if (record && record[8] < 22)
            g_skill_icon_ids[id][0] = -1 - static_cast<int>(record[8]);
        for (unsigned slot = 1; slot < 3; ++slot)
            g_skill_icon_ids[id][slot] = legacy2::menu::technique_type(
                ram.data(), ram.size(), id, slot - 1);
    }
    if (!can_select || g_skill_request.hero != hero)
        g_skill_request = {};
    g_probe_ready = true;
}

extern "C" void legacy2_clear_skill_request() {
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    g_skill_request = {};
}

extern "C" int legacy2_take_skill_request(int* hero, int* slot) {
    if (!hero || !slot) return 0;
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    const auto request = g_skill_request;
    g_skill_request = {};
    const auto age = static_cast<std::uint32_t>(g_state[4]) - request.generation;
    if (request.hero < 0 || request.hero >= 5 || request.slot < 1 ||
        request.slot > 2 || request.hero != g_state[2] || age > 120u ||
        g_skill_selection[2] != 1 ||
        g_skills[request.hero][request.slot].available != 1) return 0;
    *hero = request.hero;
    *slot = request.slot;
    return 1;
}

#if defined(__ANDROID__)
namespace {
template<class T>
jintArray make_ints(JNIEnv* env, const T* values, jsize size) {
    static_assert(sizeof(T) == sizeof(jint), "JNI integer width");
    jintArray out = env->NewIntArray(size);
    if (out && size) env->SetIntArrayRegion(out, 0, size,
        reinterpret_cast<const jint*>(values));
    return out;
}

jobjectArray make_strings(JNIEnv* env, jsize size) {
    jclass type = env->FindClass("java/lang/String");
    if (!type) return nullptr;
    jobjectArray out = env->NewObjectArray(size, type, nullptr);
    env->DeleteLocalRef(type);
    return out;
}

bool put_string(JNIEnv* env, jobjectArray out, jsize index,
        const std::u16string& value) {
    if (!out || env->ExceptionCheck()) return false;
    jstring text = env->NewString(reinterpret_cast<const jchar*>(value.data()),
        static_cast<jsize>(value.size()));
    if (!text) return false;
    env->SetObjectArrayElement(out, index, text);
    env->DeleteLocalRef(text);
    return !env->ExceptionCheck();
}

bool put_number(JNIEnv* env, jobjectArray out, jsize index, int value) {
    if (!out || env->ExceptionCheck()) return false;
    jstring text = env->NewStringUTF(std::to_string(value).c_str());
    if (!text) return false;
    env->SetObjectArrayElement(out, index, text);
    env->DeleteLocalRef(text);
    return !env->ExceptionCheck();
}

jintArray make_portrait(JNIEnv* env, const legacy2::MenuPortrait& portrait) {
    if (portrait.width <= 0 || portrait.height <= 0 ||
        static_cast<std::size_t>(portrait.width) * portrait.height >
            portrait.argb.size()) return nullptr;
    const jsize pixels = portrait.width * portrait.height;
    jintArray out = env->NewIntArray(pixels + 2);
    if (!out) return nullptr;
    const jint dimensions[] = {portrait.width, portrait.height};
    env->SetIntArrayRegion(out, 0, 2, dimensions);
    if (!env->ExceptionCheck())
        env->SetIntArrayRegion(out, 2, pixels,
            reinterpret_cast<const jint*>(portrait.argb.data()));
    return out;
}

jobjectArray make_skills(JNIEnv* env, const legacy2::menu::Skills& skills) {
    jobjectArray out = make_strings(env, 9);
    if (!out) return nullptr;
    for (jsize slot = 0; slot < 3; ++slot) {
        const auto& skill = skills[slot];
        if (!put_string(env, out, slot * 3, skill.title) ||
            !put_string(env, out, slot * 3 + 1, skill.description) ||
            !put_number(env, out, slot * 3 + 2, skill.available))
            return out;
    }
    return out;
}

jobjectArray make_items(JNIEnv* env, const legacy2::menu::ItemCatalog& items,
        const legacy2::menu::ItemCounts& counts) {
    if (counts[0] < 0) return nullptr;
    jobjectArray out = make_strings(env, legacy2::menu::kItemCount * 4);
    if (!out) return nullptr;
    for (jsize id = 0; id < static_cast<jsize>(legacy2::menu::kItemCount); ++id) {
        if (!put_number(env, out, id * 4, id) ||
            !put_string(env, out, id * 4 + 1, items[id].name) ||
            !put_string(env, out, id * 4 + 2, items[id].description) ||
            !put_number(env, out, id * 4 + 3, counts[id]))
            return out;
    }
    return out;
}

jobjectArray make_journal(JNIEnv* env, const legacy2::menu::Journal& journal) {
    jobjectArray out = make_strings(env, legacy2::menu::kJournalCount * 4);
    if (!out) return nullptr;
    for (jsize id = 0; id < static_cast<jsize>(legacy2::menu::kJournalCount); ++id) {
        const auto& entry = journal[id];
        if (!put_number(env, out, id * 4, id) ||
            !put_string(env, out, id * 4 + 1, entry.text) ||
            !put_number(env, out, id * 4 + 2, entry.status) ||
            !put_number(env, out, id * 4 + 3, entry.category))
            return out;
    }
    return out;
}
}

// One immutable publication per UI refresh. Separate getters could combine
// a character from before a state load with inventory/quests from afterwards.
// The emulator thread remains the only reader of mutable guest memory.
extern "C" JNIEXPORT jboolean JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_isPlayerActorPresent(
        JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    return (g_probe_ready && g_player_actor_present)
        ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_getMenuSnapshot(
        JNIEnv* env, jclass, jint selected) {
    legacy2::menu::StatePacket state;
    legacy2::MenuPortrait portrait;
    legacy2::menu::Skills skills{};
    legacy2::menu::ItemCatalog items;
    legacy2::menu::ItemCounts counts;
    legacy2::menu::Journal journal;
    std::array<std::int32_t, 4> skill_selection;
    std::array<std::int32_t, 3> skill_icons{{-1, -1, -1}};
    {
        std::lock_guard<std::mutex> lock(g_probe_mutex);
        if (!g_probe_ready) return nullptr;
        state = g_state;
        const auto available = [&](int id) {
            return id >= 0 && id < 6 && (state[3] & (1 << id));
        };
        if (!available(selected)) {
            selected = available(state[2]) ? state[2] : -1;
            for (int id = 0; selected < 0 && id < 6; ++id)
                if (available(id)) selected = id;
        }
        if (selected >= 0) {
            portrait = g_portraits[selected];
            skills = g_skills[selected];
            skill_icons = g_skill_icon_ids[selected];
        }
        items = g_items;
        counts = g_item_counts;
        journal = g_journal;
        skill_selection = g_skill_selection;
    }
    jclass objects = env->FindClass("java/lang/Object");
    if (!objects) return nullptr;
    jobjectArray out = env->NewObjectArray(8, objects, nullptr);
    env->DeleteLocalRef(objects);
    if (!out) return nullptr;

    // state, selected id, portrait, three techniques, 46 items, 43 quests.
    // All parts originate from the same g_generation under the same lock.
    jobject parts[8] = {};
    parts[0] = make_ints(env, state.data(), state.size());
    if (!env->ExceptionCheck()) parts[1] = make_ints(env, &selected, 1);
    if (!env->ExceptionCheck() && selected >= 0)
        parts[2] = make_portrait(env, portrait);
    if (!env->ExceptionCheck() && selected >= 0)
        parts[3] = make_skills(env, skills);
    if (!env->ExceptionCheck()) parts[4] = make_items(env, items, counts);
    if (!env->ExceptionCheck()) parts[5] = make_journal(env, journal);
    if (!env->ExceptionCheck()) parts[6] = make_ints(env, skill_selection.data(), skill_selection.size());
    if (!env->ExceptionCheck()) parts[7] = make_ints(env, skill_icons.data(), skill_icons.size());
    for (jsize i = 0; i < 8; ++i) {
        if (!env->ExceptionCheck())
            env->SetObjectArrayElement(out, i, parts[i]);
        if (parts[i]) env->DeleteLocalRef(parts[i]);
    }
    return out;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_requestSelectSkill(
        JNIEnv*, jclass, jint hero, jint slot, jint generation) {
    std::lock_guard<std::mutex> lock(g_probe_mutex);
    const auto age = static_cast<std::uint32_t>(g_state[4]) -
        static_cast<std::uint32_t>(generation);
    if (!g_probe_ready || hero < 0 || hero >= 5 || slot < 1 || slot > 2 ||
        hero != g_state[2] || generation < 0 || age > 120u ||
        g_skill_selection[2] != 1 || g_skills[hero][slot].available != 1 ||
        g_skill_request.hero >= 0) return JNI_FALSE;
    g_skill_request = {hero, slot, static_cast<std::uint32_t>(generation)};
    return JNI_TRUE;
}
extern "C" JNIEXPORT jintArray JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_getScouterSnapshot(
        JNIEnv* env, jclass) {
    legacy2::scouter::Snapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(g_probe_mutex);
        snapshot = g_scouter;
    }
    return make_ints(env, snapshot.data(), snapshot.size());
}
#endif


// Android solicita el uso; solo el hilo GBA recoge la peticion.
extern "C" int legacy2_take_item_request() {
    return g_legacy2_item_request_pending.exchange(
        -1, std::memory_order_acq_rel);
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT void JNICALL
Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_requestUseItem(
        JNIEnv*, jclass, jint item_id) {
    if (item_id < 0 || item_id >= 46)
        return;

    int empty = -1;
    g_legacy2_item_request_pending.compare_exchange_strong(
        empty, static_cast<int>(item_id),
        std::memory_order_release,
        std::memory_order_relaxed);
}
#endif
