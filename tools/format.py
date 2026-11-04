"""Apply or check the project's C formatting without configuring a build."""

import argparse
import shutil
import subprocess
import sys
import sysconfig
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report formatting errors without editing")
    parser.add_argument("files", nargs="*", type=Path, help="C files or headers; default: core, ports, and demo sources")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    sources = args.files
    if not sources:
        sources = sorted(
            path
            for directory in (root / "core", root / "ports", root / "demo")
            for path in directory.rglob("*")
            if path.is_file()
            and path.suffix in (".c", ".h")
            and not {"gen", "build"}.intersection(path.relative_to(root).parts)
        )
    for path in sources:
        if not path.is_file() or path.suffix not in (".c", ".h"):
            parser.error(f"not a C source or header file: {path}")

    executable = shutil.which("clang-format", path=sysconfig.get_path("scripts")) or shutil.which("clang-format")
    if executable is None:
        print("clang-format not found; install requirements-dev.txt into this Python environment", file=sys.stderr)
        return 1

    options = ["--dry-run", "--Werror"] if args.check else ["-i"]
    return subprocess.run(
        [executable, f"--style=file:{root / '.clang-format'}", *options, *(str(path) for path in sources)]
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
