#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "legacy2_extended_view_europe.h"
#if defined(__ANDROID__)
#include <SDL_main.h>
#endif

#include "runtime.h"
#include "mobile_platform.h"

#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

namespace {

void print_usage() {
    std::printf(
        "DragonBallZLegacyofGokuIIRecomp [--bios <path>] [--rom <path>] "
        "[game.toml]\n");
}

}  // namespace

static int run_legacy2_europe(int argc, char** argv) {
#if defined(__ANDROID__)
    setenv("GBARECOMP_TOUCH_CONTROLS", "0", 1);
#endif
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    gbarecomp::RunOptions opts;
    opts.builtin_game_name = "Dragon Ball Z: The Legacy of Goku II";
    opts.builtin_rom_sha1 = "db36ff52fcd63f753f9d66439aa3d2216701c326";
    opts.builtin_rom_crc32 = 0x20684433u;
    opts.mod_game_id = "dragon-ball-z-legacy-of-goku-ii-eu";
    opts.mod_owns_adaptive_view = false;
    // 160 authentic scanlines at the nearest whole-pixel 16:9 width.
    // Keep desktop resizing proportional while bounding ALFP to this view.
    constexpr std::uint16_t kEuropean16x9Width = (160u * 16u + 4u) / 9u;
    opts.max_view_width = kEuropean16x9Width;
    opts.max_resize_view_width = kEuropean16x9Width;
    opts.resize_driven_view = true;
    opts.extended_view_init = &legacy2::install_extended_view;
    opts.extended_view_frame = &legacy2::update_extended_view;
    opts.launcher_expose_widescreen = false;
    opts.launcher_expose_adaptive_view = false;
    opts.launcher_region = "Europe";
    opts.launcher_game_version = GBARECOMP_GAME_VERSION;
    opts.launcher_game_config = "game_europe.toml";

    std::vector<std::string> args(argv, argv + argc);
#if defined(__ANDROID__)
    // Seed native startup before loading any stale installed TOML. Explicit
    // caller/debug arguments follow these defaults and retain precedence.
    if (args.empty()) args.emplace_back("main");
    args.insert(args.begin() + 1,
                {"--view-width", "284", "--resize-view"});
#endif

    gbarecomp::MobileProcessOptions mobile;
    mobile.game_config = "variants/legacy2-europe/game.toml";
    mobile.program_name = "./DragonBallZLegacyofGokuIIRecomp";
    gbarecomp::mobile_prepare_process(args, mobile);

#if defined(__ANDROID__)
    // El runtime Android necesita las rutas explicitas.
    args.emplace_back("--bios");
    args.emplace_back("bios/gba_bios.bin");
    args.emplace_back("--rom");
    args.emplace_back("roms/legacy_of_goku_ii_europe_alfp.gba");
#endif

#if defined(__ANDROID__) && !defined(GBAGAME_RECOMP_UI)
    args.erase(
        std::remove(args.begin(), args.end(), "--no-launcher"),
        args.end());
#endif

#if defined(GBAGAME_RECOMP_UI)
    if (game_launcher_preboot(args, opts)) return 0;
#endif

    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());

    return gbarecomp::run_game(
        static_cast<int>(av.size()), av.data(), opts);
}

int main(int argc, char** argv) {
    return gbarecomp::mobile_run_with_stack(
        &run_legacy2_europe, argc, argv);
}
