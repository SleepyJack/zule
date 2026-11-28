"""Configure, build, and test every CMake preset of a project, from its presets file.

Each project's CMakePresets.json already lists what there is to build, so this
reads it rather than repeating the names: a preset added there is built wherever
this runs, including in CI. Presets marked hidden are the building blocks of the
others and are skipped, and a preset is tested when the file gives it a test
preset of the same name.
"""

import argparse
import json
import subprocess
from pathlib import Path


def presets(directory: Path) -> tuple[list[str], set[str]]:
    """Return a project's buildable preset names, and which of them have tests."""
    document = json.loads((directory / "CMakePresets.json").read_text(encoding="utf-8"))
    configure = [
        preset["name"]
        for preset in document.get("configurePresets", [])
        if not preset.get("hidden", False)
    ]
    return configure, {preset["name"] for preset in document.get("testPresets", [])}


def run(command: list[str], directory: Path) -> bool:
    """Run one step in a project directory, showing it as a shell line would."""
    print(f"\n$ cd {directory} && {' '.join(command)}", flush=True)
    return subprocess.run(command, cwd=directory, check=False).returncode == 0


def check_preset(directory: Path, preset: str, test: bool) -> bool:
    """Configure and build one preset, then test it when it has tests to run."""
    steps = [["cmake", "--preset", preset], ["cmake", "--build", "--preset", preset]]
    if test:
        steps.append(["ctest", "--preset", preset])
    return all(run(command, directory) for command in steps)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "directories", nargs="+", type=Path, help="projects, each holding a CMakePresets.json"
    )
    parser.add_argument(
        "--preset", action="append", default=[], help="build only this preset; repeatable"
    )
    parser.add_argument("--no-test", action="store_true", help="configure and build only")
    parser.add_argument(
        "--keep-going", action="store_true", help="try every preset, rather than stopping at a failure"
    )
    parser.add_argument("--list", action="store_true", help="list what would be built, and stop")
    args = parser.parse_args()

    results: list[tuple[str, bool]] = []
    for directory in args.directories:
        if not (directory / "CMakePresets.json").is_file():
            parser.error(f"no CMakePresets.json in {directory}")
        names, testable = presets(directory)
        wanted = [name for name in names if not args.preset or name in args.preset]
        missing = sorted(set(args.preset) - set(names)) if args.preset else []
        if missing and len(args.directories) == 1:
            parser.error(f"{directory} has no preset named {', '.join(missing)}")
        for preset in wanted:
            tested = preset in testable and not args.no_test
            if args.list:
                print(f"{directory}: {preset}{' with tests' if tested else ''}")
                continue
            passed = check_preset(directory, preset, tested)
            results.append((f"{directory}: {preset}", passed))
            if not passed and not args.keep_going:
                break
        if results and not results[-1][1] and not args.keep_going:
            break

    if args.list:
        return 0
    if not results:
        parser.error("no presets selected")
    print("\nPresets:")
    for name, passed in results:
        print(f"  {'pass' if passed else 'FAIL'}  {name}")
    return 0 if all(passed for _, passed in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
