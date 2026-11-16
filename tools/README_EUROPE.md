# European ALFP source generation

The existing `regen.ps1` targets the USA ROM, `game.toml`, and `generated/`.
For the European ALFP project use `regen_europe.py` with Python 3.9 or newer.
The script uses only the standard library and an already-built `gba_recompile`.
It never builds that tool, the game, or an APK.

Read-only verification:

```sh
python3 tools/regen_europe.py --check
```

This checks the imported ROM's exact ALFP SHA-1 and size, the European
configuration, and both reviewed projected-render culling instruction blocks at
`0x0801C8C8` and `0x0801C8D0`, including their current hook declarations.
It does not regenerate or modify sources.

Only after the widescreen adaptation is complete, explicitly regenerate:

```sh
python3 tools/regen_europe.py --generate
```

If the existing generator is elsewhere, pass `--tool /path/to/gba_recompile`
(or a Windows `.exe` path). `--rom` and `--root` can override their defaults.

Generation goes into a temporary staging directory. Both cull blocks and
their declarations are copied byte-for-byte from the reviewed existing
corpus, located by guest instruction address rather than shard filename.
This preserves them even if additional discovered functions move them out
of `recompiled_001.cpp`. Missing or ambiguous instruction identities stop
publication of the staged sources. Concurrent edits to the configuration
or existing corpus also stop publication.

Before publication, the complete old corpus, its SHA-256 manifest, and the
European configuration are retained in `.europe-codegen-backups/` under a
UTC timestamp. If the final directory replacement fails, the old corpus is
restored. Do not delete these backups until the new sources are verified.

The Android runtime configuration is
`android/app/src/main/assets-source/game.toml`. Keep its discovery metadata
consistent with `game_europe.toml`, while preserving its Android-specific
ROM, BIOS, save paths, and video defaults. Generated European code is shared
by the desktop and Android targets.

These two comparisons belong to the projected render predicate at
`0x0801C822`; they are not the primary overworld actor visibility checks.
The original ALFP opcodes are `0x28F0` at `0x0801C8C8` (CMP R0, #240) and
`0x4288` at `0x0801C8D0` (CMP R0, R1). The script verifies these bytes and
the reviewed widened operands and ARM flag calculations before preserving
anything. Their exact patched-block SHA-256 values are included in backups.

The earlier world-coordinate tests at `0x0801C83A` / `0x0801C840` and the
following renderer entry `0x0801C918` require separate flight/projected-scene
validation. Their full widescreen behavior has not yet been qualified; this
wrapper preserves the existing two patches without claiming broader scene
coverage.
