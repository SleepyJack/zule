"""Generate a constant C schedule table from a JSON schedule."""

import re
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any

from .validation import validate_schedule


def generate_schedule(schedule: Any, output_dir: str | Path) -> Path:
    """Validate and write the schedule C and task-ID header, returning the C path.

    Raise ValueError for invalid or unrepresentable schedules, or OSError for
    output failures. Invalid input leaves existing output untouched.
    """
    issues = validate_schedule(schedule)
    if issues:
        raise ValueError("; ".join(f"{issue.path}: {issue.message}" for issue in issues))

    tasks = schedule["tasks"]
    task_names = sorted(tasks)
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

    header_lines = [
        "/* Generated Zule task IDs. Do not edit. */",
        "#ifndef ZULE_TASK_IDS_H",
        "#define ZULE_TASK_IDS_H",
        "",
        "typedef enum {",
    ]
    for index, name in enumerate(task_names):
        header_lines.append(f"    ZULE_TASK_ID_{name.upper()} = {index},")
    header_lines.extend([
        "} zule_task_id_t;",
        "",
        "#endif",
        "",
    ])
    header = "\n".join(header_lines)

    lines = [
        "/* Generated Zule schedule. Do not edit. */",
        '#include "zule_schedule_definition.h"',
        '#include "zule_task_ids.h"',
        "",
    ]
    # Declaration order does not depend on JSON object key order.
    for name in task_names:
        lines.append(f"extern void zule_task_{name}(void);")
    lines.extend([
        "",
        "static const zule_task_t zule_tasks[] = {",
    ])
    for name in task_names:
        lines.extend([
            "    {",
            f"        .id = ZULE_TASK_ID_{name.upper()},",
            f"        .function = zule_task_{name},",
            "#if ZULE_CONFIG_TASK_NAMES",
            f'        .name = "{name}",',
            "#endif",
            "    },",
        ])
    lines.extend([
        "};",
        "",
        "static const zule_window_t zule_windows[] = {",
    ])
    for window in windows:
        name = window["task"]
        start = int(window["start_slot"])
        budget = int(tasks[name]["budget_slots"])
        lines.append(f"    {{&zule_tasks[ZULE_TASK_ID_{name.upper()}], {start}U, {budget}U}},")
    lines.extend([
        "};",
        "",
        "static const zule_schedule_t zule_schedule = {",
        f"    .slot_us = {int(schedule['slot_us'])}U,",
        f"    .cycle_slots = {int(schedule['cycle']['slots'])}U,",
        "    .windows = zule_windows,",
        "    .window_count = sizeof(zule_windows) / sizeof(zule_windows[0]),",
        "    .tasks = zule_tasks,",
        "    .task_count = sizeof(zule_tasks) / sizeof(zule_tasks[0]),",
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
    temporary_files = []
    try:
        for path, contents in ((directory / "zule_task_ids.h", header), (output, source)):
            with NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=directory, delete=False) as stream:
                temporary = Path(stream.name)
                temporary_files.append((temporary, path))
                stream.write(contents)
        for temporary, path in temporary_files:
            temporary.replace(path)
    finally:
        for temporary, _ in temporary_files:
            temporary.unlink(missing_ok=True)
    return output
