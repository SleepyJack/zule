"""Build an example for a target, run it, and check what it does.

What each example promises is a table below rather than a script per target, so
adding an example or an expectation is an edit to that table, and the same table
says what CI relies on. An example either stops by itself, and must exit zero
within its time, or runs until stopped, and must still be running when its time
is up: the two are checked differently, so each example says which it is.

Expected lines are whole lines, which must appear in the order given; a line may
repeat and later lines may come between repeats, since only the first appearance
of each is compared.
"""

import argparse
import re
import subprocess
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# QEMU's exception number for SysTick, which delivers the tick on Cortex-M.
TICK_EXCEPTION = re.compile(r"exception 15")

MODES_LINES = (
    "mode_manager: startup complete, switching to operational",
    "sense",
    "control",
    "mode_manager: work done, switching to shutdown",
    "safe_state: outputs safe, stopping the scheduler",
    "Zule modes: the scheduler stopped as requested",
)


@dataclass(frozen=True)
class Check:
    """One example on one target, and what it has to do."""

    example: str
    target: str
    seconds: float
    # Whether the example stops itself, rather than running until it is stopped.
    stops: bool = False
    # Whole lines, in the order of their first appearance.
    lines: tuple[str, ...] = ()
    # Text that must appear somewhere in the output.
    contains: tuple[str, ...] = ()
    # Least number of ticks in QEMU's exception log, for an example that prints
    # nothing: it tells a running scheduler from a stuck program.
    ticks: int = 0


CHECKS = (
    # The minimal example has no timing-sensitive checks, so it is the stable
    # smoke test; on the target it prints nothing, so its ticks are counted.
    Check("minimal", "posix", seconds=2.0),
    Check("minimal", "qemu-mps2-an385", seconds=2.0, ticks=50),
    # The showcase prints every task's name. In QEMU, with -icount and
    # sleep=off, every tick is exactly on time, so its report says so.
    Check("showcase", "posix", seconds=2.0, lines=("sense", "control", "output")),
    Check(
        "showcase",
        "qemu-mps2-an385",
        seconds=2.0,
        lines=("sense", "control", "output"),
        contains=("Tick timing: worst late 0 us, worst early 0 us",),
    ),
    # The modes example switches from the startup schedule, which has no window
    # for sense, before sense first runs, then shuts itself down. Its time is a
    # guard against a hang, not how long it runs.
    Check("modes", "posix", seconds=20.0, stops=True, lines=MODES_LINES),
    Check("modes", "qemu-mps2-an385", seconds=20.0, stops=True, lines=MODES_LINES),
)


@dataclass
class Outcome:
    """What a run did, and what was wrong with it."""

    output: str = ""
    status: int | None = None
    seconds: float = 0.0
    problems: list[str] = field(default_factory=list)


def binary(check: Check) -> Path:
    """Return the example's program, which its preset builds under build/."""
    return ROOT / "build" / "examples" / check.example / check.target / f"zule_{check.example}"


def build(check: Check) -> bool:
    """Configure and build the example for its target, regenerating its schedule."""
    directory = ROOT / "examples" / check.example
    for command in (["cmake", "--preset", check.target], ["cmake", "--build", "--preset", check.target]):
        print(f"\n$ cd {directory} && {' '.join(command)}", flush=True)
        if subprocess.run(command, cwd=directory, check=False).returncode != 0:
            return False
    return True


def command(check: Check, interrupt_log: Path | None) -> list[str]:
    """Return the command that runs the example, in QEMU for a target build."""
    if check.target == "posix":
        return [str(binary(check))]
    machine = check.target.removeprefix("qemu-")
    arguments = [
        "qemu-system-arm",
        "--machine",
        machine,
        "-nographic",
        "-monitor",
        "none",
        "-semihosting-config",
        "enable=on,target=native",
        # Guest time follows executed instructions and skips idle waits, so a
        # run is the same whatever the host is doing.
        "-icount",
        "shift=0,sleep=off",
    ]
    if interrupt_log is not None:
        arguments += ["-d", "int", "-D", str(interrupt_log)]
    return arguments + ["-kernel", str(binary(check))]


def execute(check: Check, interrupt_log: Path | None) -> Outcome:
    """Run the example for its time, and say how it ended.

    An example that stops itself is waited for; one that runs until stopped is
    expected to outlast its time and is then terminated, which is its pass.
    """
    outcome = Outcome()
    started = time.monotonic()
    # QEMU reports its own troubles on standard error, so a target run keeps
    # both streams together; on the host, standard error is the fault report.
    errors = subprocess.STDOUT if check.target != "posix" else subprocess.PIPE
    process = subprocess.Popen(
        command(check, interrupt_log), stdout=subprocess.PIPE, stderr=errors, text=True
    )
    try:
        output, host_errors = process.communicate(timeout=check.seconds)
        outcome.status = process.returncode
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            output, host_errors = process.communicate(timeout=10.0)
        except subprocess.TimeoutExpired:
            process.kill()
            output, host_errors = process.communicate()
        if check.stops:
            outcome.problems.append(f"did not stop within {check.seconds:g} s")
    outcome.seconds = time.monotonic() - started
    outcome.output = output or ""
    if check.stops:
        if outcome.status is not None and outcome.status != 0:
            outcome.problems.append(f"exited with status {outcome.status}")
    elif outcome.status is not None:
        outcome.problems.append(f"exited after {outcome.seconds:.1f} s with status {outcome.status}")
    if host_errors:
        outcome.problems.append(f"wrote to standard error: {host_errors.strip().splitlines()[0]}")
    return outcome


def check_output(check: Check, outcome: Outcome, interrupt_log: Path | None) -> None:
    """Add a problem for every expectation the run did not meet."""
    lines = outcome.output.splitlines()
    first: dict[str, int] = {}
    for wanted in check.lines:
        if wanted in lines:
            first[wanted] = lines.index(wanted)
        else:
            outcome.problems.append(f"never printed {wanted!r}")
    order = [first[wanted] for wanted in check.lines if wanted in first]
    if order != sorted(order):
        outcome.problems.append("printed the expected lines out of order")
    for wanted in check.contains:
        if wanted not in outcome.output:
            outcome.problems.append(f"output has no {wanted!r}")
    if check.ticks:
        ticking = len(TICK_EXCEPTION.findall(_read(interrupt_log)))
        if ticking < check.ticks:
            outcome.problems.append(f"{ticking} ticks in QEMU's exception log, wanted {check.ticks}")
        else:
            print(f"  {ticking} ticks in QEMU's exception log")


def _read(path: Path | None) -> str:
    if path is None or not path.is_file():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def excerpt(output: str, limit: int = 30) -> str:
    """Return the output with repeated lines collapsed, for a failure report."""
    collapsed: list[str] = []
    for line in output.splitlines():
        if collapsed and collapsed[-1].startswith(line) and collapsed[-1][len(line) :].startswith(" x"):
            repeats = int(collapsed[-1].rsplit(" x", 1)[1]) + 1
            collapsed[-1] = f"{line} x{repeats}"
        elif collapsed and collapsed[-1] == line:
            collapsed[-1] = f"{line} x2"
        else:
            collapsed.append(line)
    shown = collapsed[:limit]
    if len(collapsed) > limit:
        shown.append(f"... {len(collapsed) - limit} more lines")
    return "\n".join(f"  | {line}" for line in shown)


def report(check: Check, outcome: Outcome) -> bool:
    """Print what the run did, and whether it was what the example promises."""
    name = f"{check.example} on {check.target}"
    if outcome.problems:
        print(f"\n{name}: FAIL")
        for problem in outcome.problems:
            print(f"  {problem}")
        if outcome.output:
            print(excerpt(outcome.output))
        return False
    ended = f"stopped with 0 after {outcome.seconds:.1f} s" if check.stops else "still running"
    print(f"\n{name}: pass, {ended}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--example", action="append", default=[], help="only this example; repeatable")
    parser.add_argument("--target", action="append", default=[], help="only this target; repeatable")
    parser.add_argument("--no-build", action="store_true", help="run what is already built")
    parser.add_argument("--list", action="store_true", help="list the checks, and stop")
    args = parser.parse_args()

    selected = [
        check
        for check in CHECKS
        if (not args.example or check.example in args.example)
        and (not args.target or check.target in args.target)
    ]
    for name in args.example:
        if not any(check.example == name for check in CHECKS):
            parser.error(f"no checks for example {name}")
    for name in args.target:
        if not any(check.target == name for check in CHECKS):
            parser.error(f"no checks for target {name}")
    if args.list:
        for check in selected:
            print(f"{check.example} on {check.target}: {'stops itself' if check.stops else 'keeps running'}")
        return 0

    passed = True
    with tempfile.TemporaryDirectory() as directory:
        for check in selected:
            if not args.no_build and not build(check):
                print(f"\n{check.example} on {check.target}: FAIL\n  the build failed")
                passed = False
                continue
            if not binary(check).is_file():
                print(f"\n{check.example} on {check.target}: FAIL\n  {binary(check)} is not built")
                passed = False
                continue
            interrupt_log = Path(directory) / f"{check.example}-{check.target}.log" if check.ticks else None
            outcome = execute(check, interrupt_log)
            check_output(check, outcome, interrupt_log)
            passed &= report(check, outcome)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
