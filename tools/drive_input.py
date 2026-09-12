"""Runs a scripted input tape: the deterministic test layer of PLAN-03 section 5.2.

The tape itself lives in the build (`src/game/tape.cpp`): a list of timed input beats, named
checkpoints that assert the sim state, and named capture beats that write a frame. This script is
only the runner - it launches `Opra.exe --tape <name> --json <path>`, prints the checkpoints it read
back from the report, and exits non-zero when the tape failed. There is no window to poke and no
sleep to time: the tape's own fixed 120 Hz clock decides everything.

    python tools/drive_input.py                 # the default tape, one run
    python tools/drive_input.py flow --repeat   # run twice and prove the run is identical
"""

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

DEFAULT_TAPE = "flow"


def exe_path() -> pathlib.Path:
    """The built game, relative to this script's own tree."""
    return pathlib.Path(__file__).resolve().parent.parent / "build" / "Release" / "Opra.exe"


def run_once(exe: pathlib.Path, tape: str, out_dir: pathlib.Path) -> tuple[int, str, dict]:
    """Runs the tape with its report under `out_dir`, and returns (exit code, stdout, report)."""
    report = out_dir / f"{tape}.json"
    completed = subprocess.run(
        [str(exe), "--tape", tape, "--json", str(report)],
        capture_output=True,
        text=True,
    )
    if not report.exists():
        raise SystemExit(
            f"tape {tape}: the run wrote no report to {report}\n{completed.stdout}{completed.stderr}"
        )
    return completed.returncode, completed.stdout + completed.stderr, json.loads(
        report.read_text(encoding="utf-8")
    )


def print_report(tape: str, report: dict, stdout: str) -> None:
    """Prints the checkpoints the report carries, then the captures and their hashes."""
    print(f"tape {tape}: " + ("ok" if report["ok"] else "FAILED"))
    for at in report["checkpoints"]:
        state = (
            f"screen={at['screen']:<8} speed={at['speed']:.3f} distance={at['distance']:.3f} "
            f"ore={at['ore']:.3f} nodes={at['nodes']} docked={int(at['docked'])} "
            f"elapsed={at['elapsed']:.3f}"
        )
        mark = "ok  " if at["ok"] else "FAIL"
        print(f"  {at['name']:<8} {at['at']:5.2f}s {mark} {state}")
        if at["failure"]:
            print(f"      {at['failure']}")
    for capture in report["captures"]:
        path = pathlib.Path(capture["path"])
        digest = hashlib.sha256(path.read_bytes()).hexdigest()[:16] if path.exists() else "missing"
        print(f"  capture {capture['name']:<8} {capture['at']:5.2f}s {path.name} sha256:{digest}")
    # The tape prints where the flow goes next; pass it through so the run reads whole.
    for line in stdout.splitlines():
        if line.startswith("continuation:"):
            print(line)


def compare_runs(tape: str, first: dict, second: dict) -> bool:
    """Proves two runs reached the same state and wrote the same bytes."""
    ok = True
    if first["checkpoints"] != second["checkpoints"]:
        ok = False
        for a, b in zip(first["checkpoints"], second["checkpoints"]):
            if a != b:
                print(f"  determinism: checkpoint {a['name']} differs")
                print(f"    run 1: {a}")
                print(f"    run 2: {b}")
    captured = {c["name"]: pathlib.Path(c["path"]) for c in first["captures"]}
    for capture in second["captures"]:
        other = pathlib.Path(capture["path"])
        mine = captured.get(capture["name"])
        if mine is None or not mine.exists() or not other.exists():
            ok = False
            print(f"  determinism: capture {capture['name']} missing")
            continue
        if hashlib.sha256(mine.read_bytes()).digest() != hashlib.sha256(other.read_bytes()).digest():
            ok = False
            print(f"  determinism: capture {capture['name']} differs byte for byte")
    print(f"tape {tape}: determinism " + ("holds" if ok else "FAILED"))
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tape", nargs="?", default=DEFAULT_TAPE, help="the tape to run")
    parser.add_argument("--exe", default=None, help="the built game (default build/Release/Opra.exe)")
    parser.add_argument("--out", default=None, help="where the report and captures are written")
    parser.add_argument(
        "--repeat", action="store_true", help="run twice and assert the two runs are identical"
    )
    args = parser.parse_args()

    exe = pathlib.Path(args.exe) if args.exe else exe_path()
    if not exe.exists():
        print(f"tape runner: no game at {exe}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="opra-tape-") as scratch:
        root = pathlib.Path(args.out) if args.out else pathlib.Path(scratch)
        root.mkdir(parents=True, exist_ok=True)
        code, stdout, report = run_once(exe, args.tape, root / "run1")
        print_report(args.tape, report, stdout)

        ok = code == 0 and report["ok"]
        if args.repeat:
            code2, _, report2 = run_once(exe, args.tape, root / "run2")
            ok = compare_runs(args.tape, report, report2) and ok and code2 == 0
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
