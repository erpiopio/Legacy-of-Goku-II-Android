#!/usr/bin/env python3
"""Exercise production JNI on a host JVM; synthetic publications, no ROM/saves."""
from pathlib import Path
import os
import argparse
import hashlib
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--rom", type=Path)
parser.add_argument("--state", type=Path)
args = parser.parse_args()
if bool(args.rom) != bool(args.state):
    parser.error("--rom and --state must be supplied together")
if args.rom:
    assert hashlib.sha1(args.rom.read_bytes()).hexdigest() == "db36ff52fcd63f753f9d66439aa3d2216701c326", "ALFP identity"
    state = args.state.read_bytes()
    assert state[:4] == b"GBAS" and state[8:48] == b"db36ff52fcd63f753f9d66439aa3d2216701c326", "snapshot identity"
root = Path(__file__).resolve().parent.parent
java_home = Path(os.environ.get("JAVA_HOME", "/usr/lib/jvm/java-17-openjdk-amd64"))
with tempfile.TemporaryDirectory(prefix="legacy2-menu-jni-") as directory:
    target = Path(directory)
    sources = root / "android/app/src/main/java/org/erpiopio/legacygoku2/thor"
    subprocess.run([str(java_home/"bin/javac"), "-d", str(target),
        str(sources/"Legacy2EstadoModel.java"),
        str(sources/"Legacy2MenuSnapshot.java"),
        str(sources/"Legacy2MenuViewport.java"),
        str(root/"tools/Legacy2MenuJniTest.java")], check=True)
    library = target / "libmenu-jni-test.so"
    includes = [java_home/"include", java_home/"include/linux", root/"src"]
    includes += [root/"gbarecomp/src"/name for name in ("gba","runtime","armv4t","debug")]
    subprocess.run(["g++","-std=c++20","-O2","-shared","-fPIC","-pthread",
        "-D__ANDROID__",*[f"-I{path}" for path in includes],
        str(root/"tools/test_alfp_menu_jni.cpp"),"-o",str(library)],check=True)
    subprocess.run([str(java_home/"bin/java"),"-Xcheck:jni","-cp",str(target),
        "org.erpiopio.legacygoku2.thor.Legacy2MenuJniTest",str(library),
        *([str(args.rom.resolve()),str(args.state.resolve())] if args.rom else [])],check=True)
