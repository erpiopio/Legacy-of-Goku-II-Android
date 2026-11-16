#!/usr/bin/env python3
"""Verify a prepared Thor APK without installing, launching, or building it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import tempfile
import tomllib
import zipfile

EXPECTED_PACKAGE = 'org.erpiopio.legacygoku2.thor'
EXPECTED_CERT = '9518506e9733c4b7681146d7e3fa64ccd6c50b0235887718dbe4de2170259dfd'
EXPECTED_PAYLOAD = 'legacy2-thor-alfp-widescreen-2'
MENU_CLASS = 'Lorg/erpiopio/legacygoku2/thor/Legacy2MenuPresentation;'
MENU_JNI = 'Java_org_erpiopio_legacygoku2_thor_Legacy2MenuPresentation_getMenuSnapshot'
REQUIRED_MENU_CLASSES = {
    MENU_CLASS,
    'Lorg/erpiopio/legacygoku2/thor/Legacy2MenuSnapshot;',
    'Lorg/erpiopio/legacygoku2/thor/Legacy2MenuViewport;',
    'Lorg/erpiopio/legacygoku2/thor/Legacy2EstadoModel;',
    'Lorg/erpiopio/legacygoku2/thor/Legacy2GameActivity;',
    'Lorg/erpiopio/legacygoku2/thor/Legacy2SetupActivity;',
}
MENU_ASSETS = ('plantilla.png', 'pestana.png', 'cartel.png', 'pixel_operator.ttf')


def check_elf_arm64(data, name):
    # ELF64, little-endian, ET_DYN, EM_AARCH64. Check every packaged library.
    if (len(data) < 64 or data[:6] != b'\x7fELF\x02\x01' or
            struct.unpack_from('<HH', data, 16) != (3, 183)):
        raise ValueError(f'{name} is not an ARM64 shared ELF library')


def check_menu_dex(archive, dexdump):
    classes = {}
    with tempfile.TemporaryDirectory(prefix='thor-apk-dex-') as folder:
        for name in archive.namelist():
            if not re.fullmatch(r'classes(?:[2-9]|[1-9][0-9]+)?\.dex', name):
                continue
            path = Path(folder) / name
            path.write_bytes(archive.read(name))
            output = run(str(dexdump), str(path))
            for block in re.split(r'(?m)^Class #\d+[^\n]*\n', output):
                match = re.search(r"Class descriptor\s*:\s*'([^']+)'", block)
                if match:
                    descriptor = match.group(1)
                    if descriptor in classes:
                        raise ValueError(f'Duplicate DEX class: {descriptor}')
                    classes[descriptor] = block
    missing = REQUIRED_MENU_CLASSES - classes.keys()
    if missing:
        raise ValueError('APK has stale/incomplete menu classes: ' + ', '.join(sorted(missing)))
    method = re.search(
        r"name\s*:\s*'getMenuSnapshot'\s*\n\s*type\s*:\s*'\(I\)\[Ljava/lang/Object;'\s*\n\s*access\s*:\s*([^\n]+)",
        classes[MENU_CLASS])
    if not method or 'NATIVE' not in method.group(1):
        raise ValueError('Menu DEX lacks native getMenuSnapshot(int): Object[]')
    return sorted(REQUIRED_MENU_CLASSES)


def check_menu_native(archive, readelf):
    with tempfile.TemporaryDirectory(prefix='thor-apk-elf-') as folder:
        path = Path(folder) / 'libmain.so'
        path.write_bytes(archive.read('lib/arm64-v8a/libmain.so'))
        symbols = run(str(readelf), '--dyn-syms', '--wide', str(path))
    if not re.search(r'\bFUNC\s+GLOBAL\s+DEFAULT\s+(?!UND\b)\S+\s+' + re.escape(MENU_JNI) + r'$', symbols, re.MULTILINE):
        raise ValueError('libmain.so has stale/missing atomic menu JNI export')


def manifest_activities(manifest):
    # Keep each activity's attributes and nested intent filters together.
    result, lines, indent = [], None, None
    for line in manifest.splitlines():
        node = re.match(r'^(\s*)E: ([^ ]+)', line)
        if lines is not None and node and len(node.group(1)) <= indent:
            result.append('\n'.join(lines))
            lines = None
        if node and node.group(2) == 'activity':
            lines, indent = [line], len(node.group(1))
        elif lines is not None:
            lines.append(line)
    if lines is not None:
        result.append('\n'.join(lines))
    return result


def check_activity_contract(manifest):
    setup = EXPECTED_PACKAGE + '.Legacy2SetupActivity'
    game = EXPECTED_PACKAGE + '.Legacy2GameActivity'
    activities = {}
    for block in manifest_activities(manifest):
        match = re.search(r'android:name[^=]*="([^"]+)"', block)
        if match:
            activities[match.group(1)] = block
    if set(activities) != {setup, game}:
        raise ValueError('Unexpected activity set; dual-screen entry points are incomplete')
    if not re.search(r'android:exported[^=]*=.*0xffffffff', activities[setup]):
        raise ValueError('Setup launcher activity is not exported')
    if not re.search(r'android:exported[^=]*=.*0x0(?:\s|$)', activities[game]):
        raise ValueError('Game activity must remain private')
    if 'android.intent.action.MAIN' not in activities[setup] or \
            'android.intent.category.LAUNCHER' not in activities[setup]:
        raise ValueError('Launcher intent does not use the title setup activity')



def run(*args):
    return subprocess.check_output(args, stdin=subprocess.DEVNULL, text=True,
                                   stderr=subprocess.STDOUT)


def signer(apksigner, apk):
    output = run(str(apksigner), 'verify', '--verbose', '--print-certs', str(apk))
    certs = re.findall(r'^Signer #\d+ certificate SHA-256 digest: ([0-9a-f]+)$',
                       output, flags=re.MULTILINE)
    if certs != [EXPECTED_CERT]:
        raise ValueError('APK certificate differs from the backed-up Thor installation')
    return certs[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--original', type=Path,
                        default=Path('/tmp/legacy2-thor-state-review/original-installed.apk'))
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parent.parent,
                        help='Source tree whose exact menu assets/payload must be packaged')
    parser.add_argument('--sdk', type=Path,
                        default=Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Android/Sdk'))))
    args = parser.parse_args()
    tools = args.sdk / 'build-tools/35.0.0'
    original_cert = signer(tools / 'apksigner', args.original)
    candidate_cert = signer(tools / 'apksigner', args.apk)
    if candidate_cert != original_cert:
        raise ValueError('In-place update signature mismatch')
    badging = run(str(tools / 'aapt'), 'dump', 'badging', str(args.apk))
    match = re.search(r"^package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'",
                      badging, flags=re.MULTILINE)
    if not match or match.groups() != (EXPECTED_PACKAGE, '2', '0.1.1-widescreen-dev'):
        raise ValueError('Unexpected APK package/version identity')
    manifest = run(str(tools / 'aapt'), 'dump', 'xmltree', str(args.apk), 'AndroidManifest.xml')
    if EXPECTED_PAYLOAD not in manifest:
        raise ValueError('APK does not contain the new payload-version marker')
    check_activity_contract(manifest)
    if not re.search(r'android:allowBackup[^=]*=.*0x0(?:\s|$)', manifest):
        raise ValueError('App must preserve the existing private-data backup policy')
    readelf = args.sdk / 'ndk/27.1.12297006/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
    with zipfile.ZipFile(args.apk) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or archive.testzip() is not None:
            raise ValueError('APK ZIP contains duplicate or corrupt entries')
        private = [n for n in names if n.lower().endswith(
            ('.gba', '.agb', 'gba_bios.bin', '.sav', '.srm', '.state', '.replay'))]
        if private:
            raise ValueError('APK unexpectedly includes imported ROM/BIOS data')
        native = [n for n in names if n.startswith('lib/') and n.endswith('/libmain.so')]
        if native != ['lib/arm64-v8a/libmain.so']:
            raise ValueError('Unexpected native ABI set')
        for name in names:
            if name.startswith('lib/') and name.endswith('.so'):
                if not name.startswith('lib/arm64-v8a/'):
                    raise ValueError('Unexpected packaged ABI: ' + name)
                check_elf_arm64(archive.read(name), name)
        check_menu_native(archive, readelf)
        dex_classes = check_menu_dex(archive, tools / 'dexdump')
        asset_hashes = {}
        for name in MENU_ASSETS:
            expected = (args.source_root / 'android/app/src/main/assets' / name).read_bytes()
            actual = archive.read('assets/' + name)
            if actual != expected:
                raise ValueError('APK has missing/stale/modified supplied asset: ' + name)
            asset_hashes[name] = hashlib.sha256(actual).hexdigest()
        payload_path = 'assets/payload/variants/legacy2-europe/game.toml'
        payload = archive.read(payload_path)
        expected_payload = (args.source_root / 'android/app/src/main/assets-source/game.toml').read_bytes()
        if payload != expected_payload:
            raise ValueError('APK payload differs from the current Android game configuration')
        config = tomllib.loads(payload.decode())
        if config['program']['id'] != 'ALFP' or config['identity']['sha1'] != \
                'db36ff52fcd63f753f9d66439aa3d2216701c326':
            raise ValueError('Payload identity is not the supported ALFP ROM')
        if config['video'].get('view_width') != 284 or config['video'].get('resize_view') is not True:
            raise ValueError('Payload does not enable the adapted 284x160 field view')
    print(json.dumps({'apk': str(args.apk.resolve()), 'bytes': args.apk.stat().st_size,
                      'sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                      'certificate_sha256': candidate_cert, 'signature_matches_original': True,
                      'application_id': EXPECTED_PACKAGE, 'version_code': 2,
                      'version_name': '0.1.1-widescreen-dev', 'payload': EXPECTED_PAYLOAD,
                      'native_abi': 'arm64-v8a', 'view_width': 284,
                      'player_data_embedded': False,
                      'atomic_menu_jni_export': MENU_JNI,
                      'menu_java_signature': '(I)[Ljava/lang/Object;',
                      'menu_classes': dex_classes,
                      'supplied_assets_sha256': asset_hashes,
                      'payload_matches_current_source': True,
                      'validation_scope': 'Static APK integrity only; device/gameplay acceptance is separate'}, indent=2))


if __name__ == '__main__':
    main()
