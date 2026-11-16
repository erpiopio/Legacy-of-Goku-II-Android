# ALFP ESTADO memory layout

Verified against the user-imported European ALFP ROM, SHA-1
`db36ff52fcd63f753f9d66439aa3d2216701c326`. Read-only presentation;
changing the inspected character does not write the controlled character.

## Character records

Manager: `0x030010AC`. Its first byte is the controlled character index.
Records start at `0x030010B8`, stride `0x1C`, in order Gohan, Piccolo,
Vegeta, Trunks, Goku, Mr. Satan. All records have initial values even when
locked: populated health or experience is **not** an availability test.

| Relative offset | Meaning | Native evidence |
|---|---|---|
| `+0x00`, `+0x02` | current/max PV, little-endian 16-bit | `0800E4A4..0800E4B2` |
| `+0x04`, `+0x06` | current/max PE, little-endian 16-bit | `0800E506..0800E514` |
| `+0x0A` | FRZ, unsigned 16-bit fixed point, `>> 8` | `0800E554..0800E556` |
| `+0x0C` | POD, unsigned 16-bit fixed point, `>> 8` | `0800E55A..0800E55E` |
| `+0x0E` | RES, unsigned 16-bit fixed point, `>> 8` | `0800E560..0800E562` |
| `+0x10` | total EXP, little-endian 32-bit | `0800E6B6` |
| `+0x14` | availability/technique flags, little-endian 32-bit | below |
| `+0x18..+0x1B` | attack/ability entries, **not** FRZ/POD/RES | `0801D734..0801D73A` |

Native availability counter `08008DA4..08008DC0` iterates six records and
counts only flags `& 1`. The selected UI character must be drawn only if
this bit is set. Native current-character HP setters at `08008390..083B8`
read the manager byte then calculate `index * 0x1C + 0x0C`.

## Level and SIG

The 50 unsigned thresholds are in ROM `0x0802A6A0`. Native `08008A94`
finds the highest threshold no greater than EXP and returns index + 1.
Native `08008AB0` returns `threshold[level] - EXP`, or zero at level 50.
The native status screen calls these at `0800E438` and `0800E6CC`.
For EXP 1000 the ROM says level 6, SIG 550. No separate level byte is used.
The byte tail `01 0D 0C 02` must not be misinterpreted as these statistics.

For the controlled character only, the native status screen can replace
FRZ/POD/RES with bytes `actor+0x160..0x162` (`0800E564..0800E592`).
It queries event 7 using `0801E61A`. A verified player query handler is
`08015A30`: `cmp r1,#7` at `08015A3A`, return self at `08015B0A`.
Query container is read from `0x03001FA8`; lists are count/pointers at
`+4/+8` and `+0x40C/+0x410`, visited backwards. Read snapshots from the
emulation thread only. If the original query finds no actor, it retains
the record statistics.

## Read-only technique pages

Native dispatch `0800E7E8`: page 0 status; page 1 special melee
(`0800E294`); pages 2/3 special energy (`0800E11C`, parameter 0/1).
These page draw functions only inspect data. They do not equip an attack
or change the controlled character.

| Page | Flags mask | Unlocked title/description IDs (character 0..4) | Locked IDs |
|---|---|---|---|
| Special melee | `0x4` | `0x9D + character`, `0xA2 + character` | `0x98`, `0x99` |
| Special energy 1 | `0x8` | `0xA7 + character`, `0xAC + character` | `0x9A`, `0x9B` |
| Special energy 2 | `0x10` | `0xB1 + character`, `0xB6 + character` | `0x9A`, `0x9B` |

String lookup matches native `08026908`: read the absolute ROM pointer
at `0x086C9044 + string_id*20 + language*4`, then decode its zero-terminated
Latin-1 bytes. Languages: English, French, German, Italian, Spanish.
Mr. Satan has no authored sixth technique name/description in these groups;
never read `base+5`, which would point into an unrelated group. His initial
flags do not unlock these techniques. Display a locked/unsupported page
instead of inventing a technique for him.

Spanish native IDs `0x98/0x99`: “Ataque Especial Cuerpo a Cuerpo” /
“Esta técnica no está disponible todavía”. Trunks unlocked special melee
uses IDs `0xA0/0xA5` (“Estocada Cruzada” and its original description).

## Real snapshot cross-check

A read-only copy of the running Thor's diagnostic state was saved as
`/tmp/alfp-current-stats-proof.state`. Controlled index is 3; character flags
are `[0x40,0x40,0x40,0x41,0x40,0x40]`, so only Trunks is available.
The query container at `0x03001FA8` is `0x0202DF1C`; its first inline list
contains six objects. Reverse traversal identifies `0x0202FA80`, vtable
`0x08025AB4`, whose relative slot `+0x24` resolves to `0x08015A31`.
Its effective attribute bytes are `[11,9,10]`. The second list is empty.
The controlled record reads PV 149/149, PE 39/39, EXP 1000, level 6 and
SIG 550; its fixed-point attributes also give 11/9/10. The separate global
`0x03001390` is zero in this same state, so it is not a reliable substitute
for the native query-container lookup.

## Inventory IDs and quantities (2026-11-14 audit)

ALFP defines 50 inventory slots but only IDs 0..45 have authored entries;
46..49 are native `***` placeholders. The lower menu exposes the 46 authored
entries. Definitions are at `0x08696BC8`, stride 56 bytes. Their little-endian
u16 fields at +4/+6 contain the name/description string IDs for the common
five-language table `0x086C9044`. Spanish is pointer index 4. The order of the
text groups is **not** inventory ID order. In particular ID 0 is Alubia Senzu,
ID 2 Cáp. FRZ +1, ID 37 Libro de Matemáticas, and ID 45 Radar Dragón.

Native inventory quantity callback `0800648E..080064B0` returns the saved
byte at `0x030010AC + 0x12B + id` plus the number of active objects accepting
query event `0x17` for that ID. Pickup handler `080161A0` returns 1 when its
u32 item ID at actor+0x40 matches. Manager `0x030013B0` contains the first
count at +4 and its inline pointers at +8. The second count is at **+0x40C**
and its pointers at +0x410 (`0801E6AC..0801E6D8`); it has no additional +4
prefix. Both lists contribute. This audit corrected both a text-to-ID mapping
error and the extra +4 in the second list.

## Journal conditions and reference execution

There are 43 journal definitions at `0x086D1298`, stride 32. +4 points to
the completed condition, +8 to the pending condition, +28 to the Spanish
text, and +0 supplies the category. Native journal constructor
`0800B586..0800B5FA` evaluates completed first and pending only when not
completed. It uses the original condition evaluator `08008308`.

The authored condition corpus uses constants (`00`, `01`), unary callbacks
(`02 13`: story flag; `02 15`: inventory quantity), AND (`04`), OR (`05`),
and return (`11`). Story bits start at `0x03001160`. Extended integer
`01 84 54` decodes flag 298. Inventory-dependent conditions use the same
quantity semantics as OBJETOS, including active pickup objects. Missing
memory/counts stay unknown and cannot fabricate a completed mission.

`tools/test_alfp_menu_inventory.cpp` compares readers against the unmodified
ROM's original Thumb routines through the platform's ARM interpreter. With
`/tmp/alfp-current-stats-proof.state`, the run passed all 46 localized
name/description lookups, 14,812 item quantities and 13,846 journal statuses
across the copied real snapshot and 321 reproducible fixtures (both inline
collections; individual story flags; combined flags; counts 0/1/255).
The readers are asserted not to modify their inputs. The reference evaluator
writes its current context at `0x03001054` and uses temporary RAM in the test
process only; no ROM, save or snapshot file is written.

This is a behavioral ROM/interpreter cross-check, not an independent mGBA
oracle run, Android APK execution, touchscreen verification, or proof of
safe consumable use from JNI. Existing `test_alfp_menu_state.cpp` and
`test_alfp_menu_native.cpp` also passed the available/locked character,
PV/PE, live attributes, EXP/SIG, level-cap, skills and Spanish-accent checks.

The Spanish-only lower menu now requests language index 4 for technique
strings even when a copied save has another valid language byte. The guest
language remains unchanged. Mr. Satan always receives the original locked
technique titles/descriptions, including unsupported technique flag
combinations; no absent sixth name or empty card is emitted. These cases are
covered by `tools/test_alfp_menu_native.cpp`.
