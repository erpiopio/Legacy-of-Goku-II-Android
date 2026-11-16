#!/usr/bin/env python3
"""Run ALFP snapshots with isolated saves, strict static execution and muted audio.

Snapshots are local owner artifacts, supplied explicitly and never bundled here.
Passing this check proves only the supplied scenes and frame windows.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

ROM_SHA1 = "db36ff52fcd63f753f9d66439aa3d2216701c326"


def digest(path, algorithm="sha256"):
    h = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--state", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--width", type=int, action="append")
    parser.add_argument("--input-replay", type=Path)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    exe = (args.exe or root / "build-europe/DragonBallZLegacyofGokuIIRecomp").resolve(strict=True)
    rom = root / "roms/legacy_of_goku_ii_europe_alfp.gba"
    bios = root / "gbarecomp/bios/gba_bios.bin"
    states = [path.resolve(strict=True) for path in args.state]
    if args.frames < 1 or args.frames > 36000:
        parser.error("frames must be between 1 and 36000")
    widths = args.width or [240, 284]
    if any(width not in (240, 284) for width in widths):
        parser.error("qualified ALFP widths are 240 and 284")
    if digest(rom, "sha1") != ROM_SHA1 or rom.stat().st_size != 8388608:
        raise RuntimeError("ROM identity mismatch")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    for key in ("GBARECOMP_FORCE_INTERP", "GBARECOMP_INPUT_REPLAY", "GBARECOMP_VIEW_WIDTH",
                "GBARECOMP_WIDESCREEN", "GBARECOMP_WS_WIP"):
        env.pop(key, None)
    env.update(GBARECOMP_STRICT_STATIC="1", GBARECOMP_SELFHEAL_RECOMPILE="0")
    if args.input_replay:
        env["GBARECOMP_INPUT_REPLAY"] = str(args.input_replay.resolve(strict=True))
    sources = [root / "src/legacy2_extended_view_europe.cpp", root / "src/legacy2_projected_view.h",
               root / "src/main_europe.cpp", root / "game_europe.toml"]
    sources.extend(sorted((root / "generated_europe").glob("*.cpp")))
    report = {"checked_at_utc": datetime.now(timezone.utc).isoformat(), "rom_sha1": ROM_SHA1,
              "exe_sha256": digest(exe), "bios_sha256": digest(bios),
              "source_sha256": {str(p.relative_to(root)): digest(p) for p in sources},
              "scope": "Only listed snapshots, inputs, widths and frame windows; visual review separate.",
              "runs": []}
    for index, state in enumerate(states):
        before = digest(state)
        for width in widths:
            label = f"scene-{index:02d}-{width}"
            run_dir = out / label
            run_dir.mkdir()
            png = run_dir / "frame.png"
            final = run_dir / "final.state"
            log = run_dir / "run.log"
            cmd = [str(exe), str(root / "game_europe.toml"), "--bios", str(bios), "--rom", str(rom),
                   "--save", str(run_dir / "isolated.sav"), "--state-dir", str(run_dir / "states"),
                   "--volume", "0", "--load-state", str(state), "--frames", str(args.frames),
                   "--view-width", str(width), "--dump-png", str(png), "--dump-state", str(final)]
            with log.open("w", encoding="utf-8") as stream:
                result = subprocess.run(cmd, cwd=run_dir, env=env, stdout=stream, stderr=stream,
                                        timeout=max(90, args.frames))
            text = log.read_text(encoding="utf-8")
            coverage = bool(re.search(r"self_heal_coverage=FULLY_STATIC dispatch_misses=0 "
                                      r"interpreted_insns=0 healed_native=0", text))
            dimensions = None
            if png.exists():
                header = png.read_bytes()[:24]
                if header[:8] == b"\x89PNG\r\n\x1a\n":
                    dimensions = list(struct.unpack(">II", header[16:24]))
            window = re.search(r"bounded_run start_frame=(\d+) end_frame=(\d+)", text)
            ran_frames = int(window[2]) - int(window[1]) if window else 0
            passed = result.returncode == 0 and coverage and ran_frames == args.frames and dimensions == [width, 160]
            miss = re.search(r"STRICT_STATIC dispatch miss for pc=(0x[0-9A-Fa-f]+)", text)
            report["runs"].append({"state": str(state), "state_sha256": before, "width": width,
                                   "frames_requested": args.frames, "frames_executed": ran_frames,
                                   "exit_code": result.returncode, "coverage_fully_static": coverage,
                                   "first_missing_pc": miss[1] if miss else None,
                                   "png_dimensions": dimensions, "passed": passed,
                                   "log": str(log), "png": str(png), "final_state": str(final)})
            (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print(f"{label}: {'PASS' if passed else 'FAIL'} frames={ran_frames} strict={coverage}", flush=True)
        if digest(state) != before:
            raise RuntimeError("Input snapshot changed")
    report["passed"] = all(run["passed"] for run in report["runs"])
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
