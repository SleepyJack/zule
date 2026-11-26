"""Generate a constant C schedule table from a JSON schedule."""

import re
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any

from .validation import schedule_cycles, validate_schedule


def generate_schedule(schedule: Any, output_dir: str | Path) -> Path:
    """Validate and write the schedule C and the ID headers, returning the C path.

    Raise ValueError for invalid or unrepresentable schedules, or OSError for
    output failures. Invalid input leaves existing output untouched.
    """
    issues = validate_schedule(schedule)
    if issues:
        raise ValueError("; ".join(f"{issue.path}: {issue.message}" for issue in issues))

    tasks = schedule["tasks"]
    task_names = sorted(tasks)
    cycles = schedule_cycles(schedule)
    values = [("/slot_us", schedule["slot_us"])]
    values.extend((f"/tasks/{name}/budget_slots", task["budget_slots"]) for name, task in tasks.items())
    for _, cycle, base in cycles:
        values.append((f"{base}/slots", cycle["slots"]))
        values.extend(
            (f"{base}/windows/{i}/start_slot", window["start_slot"])
            for i, window in enumerate(cycle["windows"])
        )
    for path, value in values:
        if not 0 <= value <= 0xFFFFFFFF:
            raise ValueError(f"{path}: value does not fit uint32_t")
    for name in tasks:
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) is None:
            raise ValueError(f"/tasks: task name {name!r} cannot form a C function name")

    task_header = _id_header(
        "task", "ZULE_TASK_IDS_H", "ZULE_TASK_ID", "zule_task_id_t", task_names)
    schedule_header = _id_header(
        "schedule", "ZULE_SCHEDULE_IDS_H", "ZULE_SCHEDULE_ID", "zule_schedule_id_t",
        [name for name, _, _ in cycles])

    lines = [
        "/* Generated Zule schedule. Do not edit. */",
        '#include "zule_modules_schedule_data.h"',
        '#include "zule_modules_task_data.h"',
        '#include "zule_schedule_definition.h"',
        '#include "zule_schedule_ids.h"',
        '#include "zule_task_ids.h"',
        "",
    ]
    # Declaration order does not depend on JSON object key order.
    for name in task_names:
        lines.append(f"extern void zule_task_{name}(void);")
    # The tables are used only by zule_schedule_definition_get(), so they are defined
    # inside it (MISRA C:2012 rule 8.9); as static const objects they keep static
    # storage. Every schedule shares the one task table.
    lines.extend([
        "",
        "const zule_schedule_definition_t *zule_schedule_definition_get(void) {",
        "    static const zule_task_t zule_tasks[] = {",
    ])
    for name in task_names:
        lines.extend([
            "        {",
            f"            .id = ZULE_TASK_ID_{name.upper()},",
            f"            .function = zule_task_{name},",
            "#if ZULE_CONFIG_TASK_NAMES",
            f'            .name = "{name}",',
            "#endif",
            "        },",
        ])
    lines.append("    };")
    # Each schedule's window count is emitted as a number, for the array's size
    # and the schedule's count, so the lines stay short whatever the schedule's
    # name, and the compiler rejects more windows than the size.
    for name, cycle, _ in cycles:
        lines.extend([
            "",
            f"    static const zule_window_t zule_windows_{name}[{len(cycle['windows'])}U] = {{",
        ])
        for window in cycle["windows"]:
            task = window["task"]
            start = int(window["start_slot"])
            budget = int(tasks[task]["budget_slots"])
            lines.append(f"        {{&zule_tasks[ZULE_TASK_ID_{task.upper()}], {start}U, {budget}U}},")
        lines.append("    };")
    # Schedules are indexed by ID; the array's size is explicit because its
    # initializers are designated (MISRA C:2012 rule 9.5).
    lines.extend([
        "",
        f"    static const zule_schedule_t zule_schedules[{len(cycles)}U] = {{",
    ])
    for name, cycle, _ in cycles:
        lines.extend([
            f"        [ZULE_SCHEDULE_ID_{name.upper()}] =",
            "            {",
            f"                .cycle_slots = {int(cycle['slots'])}U,",
            f"                .windows = zule_windows_{name},",
            f"                .window_count = {len(cycle['windows'])}U,",
            "            },",
        ])
    lines.extend([
        "    };",
        "",
        "    static const zule_schedule_definition_t zule_schedule_definition = {",
        f"        .slot_us = {int(schedule['slot_us'])}U,",
        "        .tasks =",
        "            {",
        "                .entries = zule_tasks,",
        "                .count = sizeof(zule_tasks) / sizeof(zule_tasks[0]),",
        "            },",
        "        .schedules =",
        "            {",
        "                .entries = zule_schedules,",
        "                .count = sizeof(zule_schedules) / sizeof(zule_schedules[0]),",
        "            },",
        "    };",
        "",
        "    return &zule_schedule_definition;",
        "}",
        "",
        # Storage for modules that keep data for each task, sized by the task
        # count; the modules own its contents.
        "#if ZULE_MODULES_TASK_DATA",
        "zule_modules_task_data_t *zule_modules_task_data_get(void) {",
        f"    static zule_modules_task_data_t zule_modules_task_data[{len(task_names)}U];",
        "    return zule_modules_task_data;",
        "}",
        "#endif",
        "",
        # Likewise for modules that keep data for each schedule.
        "#if ZULE_MODULES_SCHEDULE_DATA",
        "zule_modules_schedule_data_t *zule_modules_schedule_data_get(void) {",
        f"    static zule_modules_schedule_data_t zule_modules_schedule_data[{len(cycles)}U];",
        "    return zule_modules_schedule_data;",
        "}",
        "#endif",
        "",
    ])
    source = "\n".join(lines)

    directory = Path(output_dir)
    directory.mkdir(parents=True, exist_ok=True)
    output = directory / "zule_schedule_definition.c"
    temporary_files = []
    try:
        outputs = (
            (directory / "zule_task_ids.h", task_header),
            (directory / "zule_schedule_ids.h", schedule_header),
            (output, source),
        )
        for path, contents in outputs:
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


def _id_header(kind: str, guard: str, prefix: str, type_name: str, names: list[str]) -> str:
    """Return a header defining an ID constant for each name, numbered in order."""
    lines = [
        f"/* Generated Zule {kind} IDs. Do not edit. */",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "typedef enum {",
    ]
    for index, name in enumerate(names):
        lines.append(f"    {prefix}_{name.upper()} = {index},")
    lines.extend([
        f"}} {type_name};",
        "",
        "#endif",
        "",
    ])
    return "\n".join(lines)
