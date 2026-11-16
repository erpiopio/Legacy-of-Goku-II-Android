# ALFP Scouter regional map

## Confirmed ROM structures

- ROM SHA-1: `db36ff52fcd63f753f9d66439aa3d2216701c326`.
- Region pointer table: `0x083039E4`, 68 entries. Descriptor at `0x08303AF4` contains count 68 and table pointer.
- Native ALFP lookup: `0x08009110`, matching the two area bytes against room records.
- Each region: compressed graphics pointer at +0, room count at +20, room pointers at +24.
- 297 room records in total: two area bytes, visited-bit index u16 at +2, eight signed rectangle coordinates at +4, followed by two u16 tilemaps at +36. The second tilemap starts after the first rectangle's tile count times two.
- DragonByteZ `decompress_container` successfully decoded all 68 graphics containers. The exporter writes a manifest, raw decoded graphics and room tilemaps. Original ROM resources are generated locally, never included in the APK.
- Native visibility logic `0x0800FF94..FFDA`: visited flags at `0x030010AC + 0xE7 + bit/8`; map-reveal flag is bit 3 at `0x030010AD`. Visited rooms use the second rectangle/tilemap, unseen revealed rooms the first.

## Player projection

- Area: u16 at `0x03000F0C`.
- Field dimensions: signed u32 at `0x03000F84`, `0x03000F88`.
- Query lists: root pointer at `0x03001FA8`; list counts/items at +4/+8 and +0x40C/+0x410.
- Player actor identified by its event-handler entry `0x08015A31`, also used by existing real-statistics decoding.
- Actor X/Y: signed fixed-point at +0x158/+0x15C. Native accessor `0x0801517E` shifts right by 8.
- Native projection `0x0801010C..1014E` uses the first room rectangle:

```
markerX = left*8 + ((right-left)*8*(actorX>>8) + width/2) / width
markerY = top*8  + ((bottom-top)*8*(actorY>>8) + height/2) / height
```

Integer division truncates toward zero, as in the original ARM routine. Initial native scroll is marker minus (120,80), as established at `0x08010168..10172`.

## Evidence

- CLI extraction succeeded for 68 regions, 297 rooms, 366 geometry/graphics/manifest files before palette/marker additions.
- Native test uses the real ALFP regional table with a synthetic actor fixture and verifies endpoints, midpoint, generation and invalid dimensions.
- Authorized Thor suspended-state copy decoded: region 41, area 4102, actor fixed `(27072,34880)`, field dimensions `(304,248)`, projected marker `(32,33)`.
- Snapshot publication is inside the existing emulation frame hook and copied under the same mutex before JNI returns it. Java does not read mutable guest memory.
- Full integration and live movement require verification on the installed APK; a successful CLI extraction or build alone does not establish those outcomes.

## Subsequent authorized task

After validating the map, reproduce original JOURNAL/FINISHED styling with original stars colored by mission category, and inventory icon/selection styling. User supplied photos `IMG20261115175505.jpg` and `IMG20261115175447.jpg` on 15 November 2026. Inspect original ALFP resources and categories rather than assigning arbitrary star colors or substituting item icons.
