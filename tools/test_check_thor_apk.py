#!/usr/bin/env python3
"""Static artifact-validator regressions; creates no APK or game data."""
from pathlib import Path
import io
import struct
import unittest
from unittest.mock import patch
import zipfile
import check_thor_apk as check

def archive(**entries):
    raw = io.BytesIO()
    with zipfile.ZipFile(raw, 'w') as output:
        for name, value in entries.items():
            output.writestr(name, value)
    raw.seek(0)
    return zipfile.ZipFile(raw)

def elf(machine=183, bits=2, endian=1):
    data=bytearray(64)
    data[:7]=b'\x7fELF'+bytes((bits,endian,1))
    struct.pack_into('<HH',data,16,3,machine)
    return data

def dex_blocks(method_type='(I)[Ljava/lang/Object;'):
    blocks=[]
    for number, name in enumerate(sorted(check.REQUIRED_MENU_CLASSES)):
        text=f"Class #{number} -\n  Class descriptor  : '{name}'\n"
        if name == check.MENU_CLASS:
            text += ("  Direct methods -\n    #0 : (in "+name+")\n"
                "      name : 'getMenuSnapshot'\n"
                f"      type : '{method_type}'\n"
                "      access : 0x010a (PRIVATE STATIC NATIVE)\n")
        blocks.append(text)
    return blocks

MANIFEST = """E: manifest
  E: application
    E: activity
      A: android:name(0x01010003)="org.erpiopio.legacygoku2.thor.Legacy2SetupActivity"
      A: android:exported(0x01010010)=(type 0x12)0xffffffff
      E: intent-filter
        E: action
          A: android:name(0x01010003)="android.intent.action.MAIN"
        E: category
          A: android:name(0x01010003)="android.intent.category.LAUNCHER"
    E: activity
      A: android:name(0x01010003)="org.erpiopio.legacygoku2.thor.Legacy2GameActivity"
      A: android:exported(0x01010010)=(type 0x12)0x0
"""

class ApkContract(unittest.TestCase):
    def test_arm64_elf(self):
        check.check_elf_arm64(elf(),'libmain.so')
        for invalid in (elf(40),elf(62),elf(bits=1),elf(endian=2),b'\x7fELF'):
            with self.assertRaises(ValueError):
                check.check_elf_arm64(invalid,'bad.so')

    def test_manifest_activity_boundaries(self):
        check.check_activity_contract(MANIFEST)
        self.assertEqual(len(check.manifest_activities(MANIFEST)),2)
        # An exported game activity must never be accepted as the launcher.
        invalid=MANIFEST.replace('=(type 0x12)0x0\n','=(type 0x12)0xffffffff\n')
        with self.assertRaises(ValueError):
            check.check_activity_contract(invalid)
        with self.assertRaises(ValueError):
            check.check_activity_contract(MANIFEST.replace('android.intent.category.LAUNCHER','wrong'))

    def test_multidex_signature(self):
        blocks=dex_blocks()
        def dump(path_tool,path_dex):
            return ''.join(blocks[:3] if Path(path_dex).name=='classes.dex' else blocks[3:])
        with archive(**{'classes.dex':b'primary','classes10.dex':b'secondary'}) as source:
            with patch.object(check,'run',side_effect=dump):
                self.assertEqual(set(check.check_menu_dex(source,'dexdump')),check.REQUIRED_MENU_CLASSES)
        with archive(**{'classes.dex':b'bad-signature'}) as source:
            with patch.object(check,'run',return_value=''.join(dex_blocks('(I)[I'))):
                with self.assertRaises(ValueError):
                    check.check_menu_dex(source,'dexdump')

    def test_duplicate_and_stale_dex(self):
        blocks=dex_blocks()
        for output in (''.join(blocks+blocks[:1]),''.join(blocks[:-1])):
            with archive(**{'classes.dex':b'invalid'}) as source:
                with patch.object(check,'run',return_value=output):
                    with self.assertRaises(ValueError):
                        check.check_menu_dex(source,'dexdump')

    def test_defined_native_export(self):
        with archive(**{'lib/arm64-v8a/libmain.so':elf()}) as source:
            with patch.object(check,'run',return_value='1: 1000 100 FUNC GLOBAL DEFAULT 14 '+check.MENU_JNI+'\n'):
                check.check_menu_native(source,'readelf')
            for output in ('', '1: 0 0 FUNC GLOBAL DEFAULT UND '+check.MENU_JNI+'\n'):
                with patch.object(check,'run',return_value=output):
                    with self.assertRaises(ValueError):
                        check.check_menu_native(source,'readelf')

if __name__=='__main__':
    unittest.main()
