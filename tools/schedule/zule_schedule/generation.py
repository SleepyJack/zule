"""Generate a constant C schedule table from a JSON schedule."""

import re
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any

from .validation import validate_schedule


def generate_schedule(schedule: Any, output_dir: str | Path) -> Path:
    """Validate and write zule_schedule_definition.c, returning its path.

    Raise ValueError for invalid or unrepresentable schedules, or OSError for
    output failures. Invalid input leaves existing output untouched.
    """
    issues = validate_schedule(schedule)
    if issues:
        raise ValueError("; ".join(f"{issue.path}: {issue.message}" for issue in issues))

    tasks = schedule["tasks"]
    windows = schedule["cycle"]["windows"]
    values = [("/slot_us", schedule["slot_us"]), ("/cycle/slots", schedule["cycle"]["slots"])]
    values.extend((f"/tasks/{name}/budget_slots", task["budget_slots"]) for name, task in tasks.items())
    values.extend((f"/cycle/windows/{i}/start_slot", window["start_slot"]) for i, window in enumerate(windows))
    for path, value in values:
        if not 0 <= value <= 0xFFFFFFFF:
            raise ValueError(f"{path}: value does not fit uint32_t")
    for name in tasks:
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) is None:
            raise ValueError(f"/tasks: task name {name!r} cannot form a C function name")

    lines = [
        "/* Generated Zule schedule. Do not edit. */",
        '#include "zule_schedule_definition.h"',
        "",
    ]
    # Declaration order does not depend on JSON object key order.
    for name in sorted({window["task"] for window in windows}):
        lines.append(f"extern void zule_task_{name}(void);")
    lines.extend([
        "",
        f"const uint32_t zule_slot_us = {int(schedule['slot_us'])}U;",
        "",
        "static const zule_window_t zule_windows[] = {",
    ])
    for window in windows:
        name = window["task"]
        start = int(window["start_slot"])
        budget = int(tasks[name]["budget_slots"])
        lines.append(f"    {{zule_task_{name}, {start}U, {budget}U}},")
    lines.extend([
        "};",
        "",
        "static const zule_schedule_t zule_schedule = {",
        f"    .cycle_slots = {int(schedule['cycle']['slots'])}U,",
        "    .windows = zule_windows,",
        "    .window_count = sizeof(zule_windows) / sizeof(zule_windows[0]),",
        "};",
        "",
        "const zule_schedule_t *zule_schedule_get(void) {",
        "    return &zule_schedule;",
        "}",
        "",
    ])
    source = "\n".join(lines)

    directory = Path(output_dir)
    directory.mkdir(parents=True, exist_ok=True)
    output = directory / "zule_schedule_definition.c"
    temporary = None
    try:
        with NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=directory, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(source)
        temporary.replace(output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return output
