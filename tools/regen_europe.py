#!/usr/bin/env python3
"""Regenerate ALFP sources explicitly, retaining the two reviewed cull patches."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

ROM_SHA1 = 'db36ff52fcd63f753f9d66439aa3d2216701c326'
ROM_INSTRUCTIONS = {'0801C8C8': 0x28F0, '0801C8D0': 0x4288}
PATCHES = {
    '0801C8C8': 'legacy2_alfp_cull_right_margin',
    '0801C8D0': 'legacy2_alfp_cull_left_margin',
}


def digest(path: Path, algorithm: str = 'sha256') -> str:
    h = hashlib.new(algorithm)
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def inventory(directory: Path) -> dict[str, str]:
    return {str(p.relative_to(directory)): digest(p)
            for p in sorted(directory.rglob('*')) if p.is_file()}


def find_instruction(directory: Path, pc: str):
    pattern = re.compile(r'^L_' + pc + r':\n.*?(?=^L_[0-9A-F]{8}:|^}\s*$)',
                         re.MULTILINE | re.DOTALL)
    found = []
    for path in sorted(directory.glob('recompiled_*.cpp')):
        text = path.read_text(encoding='utf-8')
        for match in pattern.finditer(text):
            found.append((path, text, match))
    if len(found) != 1:
        raise RuntimeError(f'{pc}: expected one instruction block, found {len(found)}')
    return found[0]


def verify_operands(pc: str, block: str):
    compact = re.sub(r'\s+', '', block)
    if pc == '0801C8C8':
        expected = [
            'uint32_t_rn_0801C8C8=g_cpu.R[0];',
            'constuint32_t_limit_0801C8C8=0x000000F0u+legacy2_alfp_cull_right_margin();',
            '_r_0801C8C8=_rn_0801C8C8-_limit_0801C8C8;',
            'arm_set_nzcv_sub(_rn_0801C8C8,_limit_0801C8C8,_r_0801C8C8);',
        ]
    else:
        expected = [
            'uint32_t_rm_0801C8D0=(g_cpu.R[1]-legacy2_alfp_cull_left_margin());',
            '_op2_0801C8D0=_rm_0801C8D0;',
            'uint32_t_rn_0801C8D0=g_cpu.R[0];',
            '_r_0801C8D0=_rn_0801C8D0-_op2_0801C8D0;',
            'arm_set_nzcv_sub(_rn_0801C8D0,_op2_0801C8D0,_r_0801C8D0);',
        ]
    if any(fragment not in compact for fragment in expected):
        raise RuntimeError(f'{pc}: reviewed comparison operands or ARM flags changed')
    if compact.count(PATCHES[pc] + '()') != 1:
        raise RuntimeError(f'{pc}: expected exactly one margin call')


def capture_patches(directory: Path):
    if not (directory / 'dispatch_table.cpp').is_file() or not (directory / 'recompiled.h').is_file():
        raise RuntimeError('Incomplete generated_europe corpus')
    patches = {}
    declarations = {}
    for pc, hook in PATCHES.items():
        path, text, match = find_instruction(directory, pc)
        block = match.group()
        if hook + '()' not in block or 'arm_set_nzcv_sub' not in block:
            raise RuntimeError(f'{path.name}:{pc}: reviewed cull patch missing')
        if f'/* {pc} ' not in block or f'runtime_tick(_cyc_{pc})' not in block:
            raise RuntimeError(f'{path.name}:{pc}: unexpected instruction layout')
        proto = re.compile(r'^extern "C"[^;\n]*\b' + hook + r'\(\);[ \t]*$', re.MULTILINE)
        matches = proto.findall(text)
        if len(matches) != 1:
            raise RuntimeError(f'{path.name}:{pc}: expected one declaration for {hook}')
        verify_operands(pc, block)
        patches[pc] = block
        declarations[hook] = matches[0].rstrip()
    return patches, declarations


def transplant(directory: Path, patches, declarations):
    # Locate by guest PC rather than shard number. A larger discovery corpus
    # can move these instructions out of recompiled_001.cpp.
    for pc, block in patches.items():
        path, text, match = find_instruction(directory, pc)
        if f'/* {pc} ' not in match.group():
            raise RuntimeError(f'{pc}: generated instruction identity changed')
        path.write_text(text[:match.start()] + block + text[match.end():], encoding='utf-8')
    for path in directory.glob('recompiled_*.cpp'):
        text = path.read_text(encoding='utf-8')
        prefix = []
        for hook, declaration in declarations.items():
            if hook + '()' in text and declaration not in text:
                prefix.append(declaration)
        if prefix:
            path.write_text('\n'.join(prefix) + '\n' + text, encoding='utf-8')
    result, result_declarations = capture_patches(directory)
    if result != patches or result_declarations != declarations:
        raise RuntimeError('Cull preservation verification failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true', help='read-only identity and existing patch verification')
    mode.add_argument('--generate', action='store_true', help='explicitly regenerate into staging, verify, then back up and replace')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--rom', type=Path)
    parser.add_argument('--tool', type=Path)
    parser.add_argument('--max-functions', type=int, default=65536)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    corpus = root / 'generated_europe'
    rom = (args.rom or root / 'roms/legacy_of_goku_ii_europe_alfp.gba').resolve(strict=True)
    config = root / 'game_europe.toml'
    if rom.stat().st_size != 8388608 or digest(rom, 'sha1') != ROM_SHA1:
        raise RuntimeError('ROM is not the verified European ALFP dump')
    with rom.open('rb') as stream:
        for pc, opcode in ROM_INSTRUCTIONS.items():
            stream.seek(int(pc, 16) - 0x08000000)
            if struct.unpack('<H', stream.read(2))[0] != opcode:
                raise RuntimeError(f'{pc}: original ALFP Thumb CMP opcode changed')
    config_text = config.read_text(encoding='utf-8')
    if ROM_SHA1 not in config_text or 'id = "ALFP"' not in config_text:
        raise RuntimeError('Expected European ALFP configuration')
    patches, declarations = capture_patches(corpus)
    original = inventory(corpus)
    original_config = digest(config)
    print('ALFP identity verified; both reviewed cull instruction blocks and declarations verified.')
    if args.check:
        print('Read-only check complete; no generation, compilation, or source writes performed.')
        return
    candidates = [root / 'gbarecomp/build/gba_recompile', root / 'gbarecomp/build/gba_recompile.exe']
    tool = args.tool or next((p for p in candidates if p.is_file()), None)
    if tool is None:
        raise RuntimeError('Existing gba_recompile tool not found; pass --tool. This script never builds it.')
    tool = tool.resolve(strict=True)
    if args.max_functions < 1:
        raise RuntimeError('--max-functions must be positive')
    # Never generate directly over hand-reviewed source. Keep the staging
    # output until all identity, PC, patch and concurrent-edit checks pass.
    with tempfile.TemporaryDirectory(prefix='.alfp-codegen-stage-', dir=root) as tmp:
        staging = Path(tmp) / 'generated_europe'
        subprocess.run([str(tool), '--rom', str(rom), '--config', str(config),
                        '--out', str(staging), '--max-functions', str(args.max_functions)],
                       cwd=root, check=True)
        transplant(staging, patches, declarations)
        if inventory(corpus) != original or digest(config) != original_config:
            raise RuntimeError('Sources or configuration changed while generating; existing corpus left intact')
        backup_root = root / '.europe-codegen-backups'
        backup_root.mkdir(exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        backup = backup_root / stamp
        backup.mkdir()
        shutil.copy2(config, backup / 'game_europe.toml')
        manifest = {
            'rom_sha1': ROM_SHA1,
            'instructions': {
                pc: {'thumb_opcode': f'0x{ROM_INSTRUCTIONS[pc]:04X}',
                     'hook': PATCHES[pc],
                     'reviewed_block_sha256': hashlib.sha256(block.encode('utf-8')).hexdigest()}
                for pc, block in patches.items()},
            'corpus': original,
        }
        (backup / 'original-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        old = backup / 'generated_europe'
        os.replace(corpus, old)
        try:
            os.replace(staging, corpus)
        except BaseException:
            os.replace(old, corpus)
            raise
        print(f'European corpus updated; previous complete corpus preserved at {old}')
        print('Both culls retained byte-for-byte. No compilation was performed.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'ALFP regeneration stopped: {error}')
