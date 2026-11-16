#!/usr/bin/env python3
"""Inspect ALFP authored map resources without building or changing the game.

The Python decoder follows the ROM format. Supplying field-gameplay GBAS states
also compares its output with the original game's decoded EWRAM cache slots,
checks authored screen tiles against native VRAM, and verifies character tile
zero in active field layers. A ROM scan alone validates resource streams only.
"""
import argparse
import collections
import glob
import hashlib
import json
from pathlib import Path
import struct

ROM_BASE = 0x08000000
ROM_SHA1 = "db36ff52fcd63f753f9d66439aa3d2216701c326"
ROM_SIZE = 8388608
RESOURCE_CONSTRUCTOR = 0x080056F9
FIELD_TYPE = 0x08023D18
CHUNK_SIZE = 0x800


def u32(data, offset):
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("word outside data")
    return struct.unpack_from("<I", data, offset)[0]


class BitReader:
    """Little-endian 32-bit input words, consumed most significant bit first."""
    def __init__(self, rom, offset):
        self.rom, self.offset = rom, offset
        self.word = self.remaining = 0

    def bits(self, count):
        if not 0 <= count <= 31:
            raise ValueError("invalid bit count")
        result = 0
        for _ in range(count):
            if not self.remaining:
                self.word = u32(self.rom, self.offset)
                self.offset += 4
                self.remaining = 32
            result = (result << 1) | (self.word >> 31)
            self.word = (self.word << 1) & 0xFFFFFFFF
            self.remaining -= 1
        return result

    def gamma(self):
        value = 1
        for _ in range(31):
            value = (value << 1) | self.bits(1)
            if not self.bits(1):
                return value
        raise ValueError("unbounded gamma code")


def decode_chunk(rom, descriptor):
    offset = descriptor - ROM_BASE
    kind, output_size = u32(rom, offset), u32(rom, offset + 4)
    if output_size != CHUNK_SIZE:
        raise ValueError("unexpected chunk output size")
    if kind == 0:
        result = rom[offset + 8:offset + 8 + output_size]
        if len(result) != output_size:
            raise ValueError("truncated raw chunk")
        return result
    if kind not in (1, 2):
        raise ValueError("unsupported chunk compression")
    reader = BitReader(rom, offset + 8)
    output = bytearray()
    distance_bits, literal_bits, literal_base, last_distance = 8, kind, 0, 1

    def literal(value):
        if len(output) >= output_size:
            raise ValueError("literal exceeds chunk output")
        output.append(value & 255)

    def match(distance, length):
        if not 0 < distance <= len(output) or length > output_size - len(output):
            raise ValueError("invalid chunk backreference")
        for _ in range(length):
            output.append(output[-distance])

    for _ in range(65536):
        if reader.bits(1):
            literal(reader.bits(literal_bits) + literal_base)
            continue
        if reader.bits(1):
            code = reader.gamma()
            if code == 2:
                distance, length = last_distance, reader.gamma()
            else:
                if code < 3 or distance_bits > 24:
                    raise ValueError("invalid long match code")
                distance = reader.bits(distance_bits) + ((code - 3) << distance_bits)
                last_distance, length = distance, reader.gamma()
                if distance >= 0x10000:
                    length += 3
                elif distance >= 0x37FF:
                    length += 2
                elif distance >= 0x027F:
                    length += 1
                elif distance <= 0x007F:
                    length += 4
            match(distance, length)
            continue
        if reader.bits(1) == 0:
            distance = reader.bits(7)
            if distance:
                last_distance = distance
                match(distance, reader.bits(2) + 2)
            else:
                code = reader.bits(2)
                if code == 0:
                    if len(output) != output_size:
                        raise ValueError("terminator before complete output")
                    return bytes(output)
                distance_bits = reader.bits(code + 3)
            continue
        short_code = reader.bits(4) - 1
        if short_code == 0:
            literal(0)
        elif short_code > 0:
            match(short_code, 1)
        elif reader.bits(1):
            while True:
                for _ in range(256):
                    literal(reader.bits(8))
                if not reader.bits(1):
                    break
        else:
            literal_base, literal_bits = 0, 7 + reader.bits(1)
            if literal_bits == 7:
                literal_base = reader.bits(8)
    raise ValueError("chunk command limit exceeded")


def resources(rom):
    signature = struct.pack("<I", RESOURCE_CONSTRUCTOR)
    found, offset = {}, rom.find(signature)
    while offset >= 0:
        if not offset % 4 and offset + 24 <= len(rom):
            columns, rows = rom[offset + 20:offset + 22]
            count = columns * rows
            layer = (u32(rom, offset + 16) >> 16) & 255
            if layer < 4 and 0 < count <= 255 and offset + 24 + count * 4 <= len(rom):
                descriptors = tuple(u32(rom, offset + 24 + i * 4) for i in range(count))
                found[ROM_BASE + offset] = (columns, rows, layer, descriptors)
        offset = rom.find(signature, offset + 1)
    if not found:
        raise ValueError("no ALFP map resources found")
    return found


def state_sections(path):
    data = Path(path).read_bytes()
    if data[:4] != b"GBAS" or data[8:48].decode("ascii") != ROM_SHA1:
        raise ValueError("snapshot is not for this ALFP ROM")
    result, offset = {}, 52
    for _ in range(u32(data, 48)):
        tag, length = data[offset:offset + 4], u32(data, offset + 4)
        offset += 8
        if tag in result or offset + length > len(data):
            raise ValueError("invalid snapshot section framing")
        result[tag] = data[offset:offset + length]
        offset += length
    if offset != len(data):
        raise ValueError("snapshot trailing bytes")
    return result


def check_native_screen(sections, catalog, decoded, objects):
    """Compare authored tiles against the original game's native VRAM ring.

    Full BG scroll words are intentional: ALFP's different per-layer VOFS
    values match native tiles. Subtracting 64 * BG does not match the game.
    Coordinates outside a resource are excluded because the runtime preserves
    the native layer there (including temporary dialogue graphics).
    """
    bus, io = sections[b"BUS0"], sections[b"IO_0"]
    vram = bus[0x48400:0x60400]
    if len(vram) != 0x18000 or len(io) < 0x400:
        raise ValueError("snapshot lacks complete VRAM or IO data")
    dispcnt = struct.unpack_from("<H", io)[0]
    enabled = (dispcnt >> 8) & 15
    layout_ok = (dispcnt & 0x00C7) == 0x0040 and enabled != 0
    for layer in range(4):
        if not enabled & (1 << layer):
            continue
        cnt = struct.unpack_from("<H", io, 8 + layer * 2)[0]
        layout_ok = layout_ok and (cnt >> 8) & 31 == 28 + layer and cnt >> 14 == 0
    if not layout_ok:
        return {"comparison": "not exercised", "reason": "not the ALFP field video layout"}

    layers, total = [], 0
    for obj in objects:
        layer, resource = obj["layer"], int(obj["resource"], 16)
        if not enabled & (1 << layer):
            continue
        columns, rows, _, descriptors = catalog[resource]
        cnt = struct.unpack_from("<H", io, 8 + layer * 2)[0]
        scroll_x, scroll_y = struct.unpack_from("<2H", io, 16 + layer * 4)
        screen_base = ((cnt >> 8) & 31) * 0x800
        compared, outside, null_entries = 0, 0, 0
        for y in range(0, 160, 8):
            for x in range(0, 240, 8):
                world_x, world_y = scroll_x + x, scroll_y + y
                if world_x >= columns * 256 or world_y >= rows * 256:
                    outside += 1
                    continue
                chunk_x, chunk_y = world_x // 256, world_y // 256
                descriptor = descriptors[chunk_y * columns + chunk_x]
                if descriptor:
                    tile_offset = (((world_y // 8) % 32) * 32 +
                                   (world_x // 8) % 32) * 2
                    expected = struct.unpack_from("<H", decoded[descriptor], tile_offset)[0]
                else:
                    expected = 0
                    null_entries += 1
                ring_offset = screen_base + ((((scroll_y + y) // 8) % 32) * 32 +
                                              ((scroll_x + x) // 8) % 32) * 2
                actual = struct.unpack_from("<H", vram, ring_offset)[0]
                if actual != expected:
                    raise ValueError("native tilemap mismatch BG%d at screen (%d,%d): "
                                     "VRAM=%04X authored=%04X" %
                                     (layer, x, y, actual, expected))
                compared += 1
        char_base = ((cnt >> 2) & 3) * 0x4000
        tile_bytes = 64 if cnt & 0x80 else 32
        tile_zero = vram[char_base:char_base + tile_bytes]
        if len(tile_zero) != tile_bytes or any(tile_zero):
            raise ValueError("BG%d character tile zero is not transparent" % layer)
        layers.append({"layer": layer, "scroll": [scroll_x, scroll_y],
                       "screen_base": hex(screen_base), "character_base": hex(char_base),
                       "native_tilemap_entries_compared": compared,
                       "outside_authored_resource": outside,
                       "null_descriptor_entries": null_entries,
                       "tile_zero_bytes": tile_bytes, "tile_zero_all_zero": True})
        total += compared
    return {"comparison": "passed" if total else "not exercised",
            "native_tilemap_entries_compared": total, "layers": layers}


def check_state(path, catalog, decoded):
    sections = state_sections(path)
    ewram = sections[b"BUS0"][:0x40000]
    objects, compared, null_owned = [], 0, 0
    for offset in range(0, len(ewram) - 0x2028 + 1, 4):
        if u32(ewram, offset) != FIELD_TYPE:
            continue
        resource = u32(ewram, offset + 8)
        if resource not in catalog:
            raise ValueError("field type points to unknown resource")
        columns, rows, layer, descriptors = catalog[resource]
        if ewram[offset + 4] != layer:
            raise ValueError("field object and resource layer disagree")
        objects.append({"payload": hex(0x02000000 + offset), "layer": layer,
                        "resource": hex(resource), "dimensions": [columns, rows]})
        for slot, owner in enumerate(ewram[offset + 0x1C:offset + 0x20]):
            if owner == 255:
                continue
            if owner >= len(descriptors):
                raise ValueError("field cache owner outside resource")
            descriptor = descriptors[owner]
            if descriptor == 0:
                # The original uses a separate empty-plane pointer for nulls.
                null_owned += 1
                continue
            actual = ewram[offset + 0x28 + slot * CHUNK_SIZE:
                           offset + 0x28 + (slot + 1) * CHUNK_SIZE]
            if actual != decoded[descriptor]:
                raise ValueError("native cache mismatch at payload %08X slot %d" %
                                 (0x02000000 + offset, slot))
            compared += 1
    return {"state": str(path), "objects": objects, "native_slots_compared": compared,
            "null_owned_slots": null_owned,
            "native_comparison": "passed" if compared else "not exercised",
            "native_screen": check_native_screen(sections, catalog, decoded, objects)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path,
                        default=Path(__file__).resolve().parent.parent /
                        "roms/legacy_of_goku_ii_europe_alfp.gba")
    parser.add_argument("--states", nargs="*", default=[], help="GBAS file paths or globs")
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    digest = hashlib.sha1(rom).hexdigest()
    if len(rom) != ROM_SIZE or digest != ROM_SHA1 or rom[0xAC:0xB0] != b"ALFP":
        raise ValueError("ROM identity does not match supported European ALFP")
    # 08005432/08005348 load this zero plane for null descriptors.
    empty_plane = u32(rom, 0x55E0)
    empty_offset = empty_plane - ROM_BASE
    if not 0 <= empty_offset <= len(rom) - CHUNK_SIZE or any(rom[empty_offset:empty_offset + CHUNK_SIZE]):
        raise ValueError("native null-resource plane is not a complete zero chunk")
    catalog = resources(rom)
    references = [d for *_, ds in catalog.values() for d in ds]
    unique = sorted(set(references) - {0})
    decoded = {d: decode_chunk(rom, d) for d in unique}
    zero_entries = collections.Counter()
    for data in decoded.values():
        zero_entries.update(v for v in struct.unpack("<1024H", data) if v & 1023 == 0)
    state_paths = sorted({p for pattern in args.states for p in glob.glob(pattern)})
    report = {"rom_sha1": digest, "rom_code": "ALFP", "resources": len(catalog),
              "resources_by_layer": dict(collections.Counter(v[2] for v in catalog.values())),
              "unique_descriptors_decoded": len(decoded),
              "null_descriptor_references": references.count(0),
              "native_null_plane": hex(empty_plane),
              "native_null_plane_all_zero": True,
              "compression_kinds": dict(collections.Counter(u32(rom, d - ROM_BASE) for d in unique)),
              "tile_index_zero_map_entries": dict(zero_entries),
              "states": [check_state(p, catalog, decoded) for p in state_paths],
              "limitation": "Native cache, tilemap, and character-zero checks require "
                            "field gameplay snapshots; a ROM scan alone is not visual QA."}
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
