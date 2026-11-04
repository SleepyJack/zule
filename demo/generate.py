"""Generate the demo schedule using the current Python interpreter."""

import subprocess
import sys
from pathlib import Path


def main() -> int:
    demo_dir = Path(__file__).resolve().parent
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "zule_schedule",
            "generate",
            str(demo_dir / "control-loop.json"),
            str(demo_dir / "gen"),
        ],
        cwd=demo_dir.parent / "tools" / "schedule",
    )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
