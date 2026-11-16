cmake_minimum_required(VERSION 3.20)
project(DragonBallZLegacyofGokuIIRecomp VERSION 0.0.4 LANGUAGES C CXX)
set(GBARECOMP_GAME_VERSION "${PROJECT_VERSION}" CACHE STRING "Player-visible game build version")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to this game's gbarecomp worktree")
if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "GBARECOMP_ROOT is not a gbarecomp checkout: ${GBARECOMP_ROOT}")
endif()

if(ANDROID)
    # Android utiliza su propia interfaz Java, sin ImGui.
    set(GBARECOMP_RUNTIME_UI_ROOT "" CACHE PATH
        "recomp-ui root used by the live GBA runtime adapter" FORCE)
else()
    set(GBARECOMP_RUNTIME_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui"
        CACHE PATH "recomp-ui root used by the live GBA runtime adapter")
endif()
set(RECOMP_UI_ROOT "${GBARECOMP_RUNTIME_UI_ROOT}"
    CACHE PATH "Path to the shared recomp-ui checkout")
set(GBARECOMP_ENABLE_MODS ON CACHE BOOL
    "Enable the built-in Legacy of Goku II mod catalog" FORCE)

add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/recompiled.cpp")
    message(FATAL_ERROR
        "Stale monolithic generated/recompiled.cpp found. Run tools/regen.ps1.")
endif()
option(LEGACY2_EUROPE "Build European ALFP version" OFF)

if(LEGACY2_EUROPE)
    set(LEGACY2_MAIN src/main_europe.cpp)
    set(LEGACY2_EXTRA_SOURCES
        src/legacy2_extended_view_europe.cpp
        src/legacy2_menu_bridge.cpp
        src/legacy2_menu_portraits.cpp)
    set(LEGACY2_GENERATED_DIR generated_europe)
    set(LEGACY2_CONFIG game_europe.toml)
else()
    set(LEGACY2_MAIN src/main.cpp)
    set(LEGACY2_EXTRA_SOURCES
        src/legacy2_extended_view.cpp
        src/mods/legacy2_adaptive_view_plugin.cpp)
    set(LEGACY2_GENERATED_DIR generated)
    set(LEGACY2_CONFIG game.toml)
endif()

file(GLOB GAME_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/${LEGACY2_GENERATED_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/${LEGACY2_GENERATED_DIR}/*.cpp")

if(ANDROID)
    set(LEGACY2_TARGET main)
    add_library(${LEGACY2_TARGET} SHARED)
    target_link_libraries(${LEGACY2_TARGET} PRIVATE SDL2::SDL2 log android)
    target_link_options(${LEGACY2_TARGET} PRIVATE "-Wl,--no-undefined")
else()
    set(LEGACY2_TARGET DragonBallZLegacyofGokuIIRecomp)
    add_executable(${LEGACY2_TARGET})
endif()

target_sources(${LEGACY2_TARGET} PRIVATE
    ${LEGACY2_MAIN}
    ${LEGACY2_EXTRA_SOURCES}
    ${GAME_GENERATED_SOURCES})

if(ANDROID AND LEGACY2_EUROPE)
    target_sources(${LEGACY2_TARGET} PRIVATE
        src/thor_maps/legacy2_scouter_extract.cpp
        src/thor_maps/legacy2_scouter_jni.cpp
        src/thor_maps/legacy2_items_extract.cpp
        src/thor_maps/legacy2_items_jni.cpp
        src/thor_maps/legacy2_skills_extract.cpp
        src/thor_maps/legacy2_skills_jni.cpp
        src/thor_maps/dragonbytez/src/rom.cpp
        src/thor_maps/dragonbytez/src/compression.cpp)
    target_include_directories(${LEGACY2_TARGET} PRIVATE
        src/thor_maps/dragonbytez/include)
endif()

if(LEGACY2_EUROPE AND GBARECOMP_RUNTIME_UI_ROOT)
    target_sources(${LEGACY2_TARGET} PRIVATE
        "${GBARECOMP_RUNTIME_UI_ROOT}/src/common/recomp_runtime_ui.c"
        "${GBARECOMP_RUNTIME_UI_ROOT}/src/common/recomp_runtime_settings.c")
endif()


set(GAME_RECOMPILED_SOURCES ${GAME_GENERATED_SOURCES})
list(FILTER GAME_RECOMPILED_SOURCES INCLUDE REGEX
    "/recompiled_[0-9]+\\.cpp$")
if(GAME_RECOMPILED_SOURCES)
    set_source_files_properties(${GAME_RECOMPILED_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-O1;-g0")
endif()

target_include_directories(${LEGACY2_TARGET} PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug"
    src)

target_link_libraries(${LEGACY2_TARGET} PRIVATE
    "-Wl,--start-group"
    gbarecomp_runtime
    gbarecomp_debug
    gbarecomp_gba
    gbarecomp_armv4t
    "-Wl,--end-group")

target_compile_definitions(${LEGACY2_TARGET} PRIVATE
    GBARECOMP_GAME_VERSION="${GBARECOMP_GAME_VERSION}"
    GBARECOMP_DEFAULT_DEBUG_PORT=19888
    GBARECOMP_WINDOW_TITLE="DragonBallZLegacyofGokuIIRecomp"
    GBARECOMP_DEFAULT_GAME_CONFIG="${LEGACY2_CONFIG}")

option(GBAGAME_RECOMP_UI "Build the recomp-ui pre-boot launcher" ON)
if(GBAGAME_RECOMP_UI)
    include(${RECOMP_UI_ROOT}/recomp_ui.cmake)
    add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
    target_include_directories(game_launcher_ui PRIVATE
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/gba"
        src)
    target_compile_definitions(game_launcher_ui PRIVATE
        GBARECOMP_ENABLE_MODS=1)
    recomp_target_launcher_ui(game_launcher_ui
        CONSOLE gba
        BOXART ${CMAKE_CURRENT_SOURCE_DIR}/recomp/launcher/boxart.tga)
    find_package(SDL2 REQUIRED)
    target_link_libraries(game_launcher_ui PUBLIC SDL2::SDL2)
    if(WIN32)
        target_link_libraries(game_launcher_ui PUBLIC opengl32)
    endif()
    target_link_libraries(
        ${LEGACY2_TARGET} PRIVATE game_launcher_ui)
    set_property(SOURCE ${LEGACY2_MAIN} APPEND PROPERTY
        COMPILE_DEFINITIONS GBAGAME_RECOMP_UI)
endif()

set(LEGACY2_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")
if(NOT LEGACY2_EUROPE AND EXISTS "${LEGACY2_PRELOADED_MODS}/packages")
    file(GLOB_RECURSE LEGACY2_MOD_MANIFESTS CONFIGURE_DEPENDS
        "${LEGACY2_PRELOADED_MODS}/packages/*.toml")
    set_property(TARGET DragonBallZLegacyofGokuIIRecomp APPEND PROPERTY
        LINK_DEPENDS ${LEGACY2_MOD_MANIFESTS})
    add_custom_command(TARGET ${LEGACY2_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${LEGACY2_PRELOADED_MODS}"
            "$<TARGET_FILE_DIR:${LEGACY2_TARGET}>/mods"
        COMMENT "Preloading the Legacy of Goku II mod catalog")
endif()

if(MSVC)
    target_link_options(
        ${LEGACY2_TARGET} PRIVATE /STACK:16777216)
elseif(MINGW OR WIN32)
    target_link_options(${LEGACY2_TARGET} PRIVATE
        -Wl,--stack,16777216)
endif()
