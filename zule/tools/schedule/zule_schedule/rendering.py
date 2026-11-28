"""Render a schedule file as a task list and a slot grid.

The tasks and their budgets come first, since a file's tasks are shared by every
schedule in it, and each schedule follows as a grid of slots with one lane per
task it places. A cycle too long for the terminal continues on the next stave,
where the editor scrolls instead; both draw on the same [grid](grid.py).

Rendering is a pure function of the schedule, so it is testable without a
terminal, and it tolerates schedules that fail the semantic checks: windows that
overlap or reach past the cycle still render, since seeing them is the point.
It requires the structure the schema defines; see schema_issues().
"""

from typing import Any

from .grid import IDLE, SLOT_COLUMNS, bar, coverage, idle_slots, label, label_width, lane, ruler
from .validation import schedule_cycles

_RESET = "\033[0m"
_DIM = "\033[2m"
_LANE_COLOURS = ("\033[36m", "\033[33m", "\033[32m", "\033[35m", "\033[34m", "\033[31m")


def render_schedule(schedule: Any, *, width: int = 80, color: bool = False) -> str:
    """Return the file's tasks, then each schedule's grid in schedule ID order."""
    tasks = schedule["tasks"]
    names = sorted(tasks)
    labels = label_width(names)
    colours = {name: _LANE_COLOURS[index % len(_LANE_COLOURS)] for index, name in enumerate(names)}
    blocks = [_task_block(tasks, names, labels, width, colours, color)]
    blocks.extend(
        _cycle_block(name, cycle, tasks, schedule["slot_us"], labels, width, colours, color)
        for name, cycle, _ in schedule_cycles(schedule)
    )
    return "\n\n".join(blocks)


def _task_block(
    tasks: Any, names: list[str], labels: int, width: int, colours: Any, color: bool
) -> str:
    """Return the task list, each budget in slots and then drawn as a window."""
    counts = {name: _slots(tasks[name]["budget_slots"]) for name in names}
    column = max([len(count) for count in counts.values()] + [0]) + 2
    room = max(width - labels - column, SLOT_COLUMNS)
    lines = [_paint(f"tasks · {_count(len(names), 'task')}", _DIM, color)]
    for name in names:
        line = label(name, labels) + counts[name].ljust(column)
        lines.append(_paint(line + bar(tasks[name]["budget_slots"])[:room], colours[name], color))
    return "\n".join(line.rstrip() for line in lines)


def _cycle_block(
    name: str,
    cycle: Any,
    tasks: Any,
    slot_us: int,
    labels: int,
    width: int,
    colours: Any,
    color: bool,
) -> str:
    """Return one schedule's grid, wrapped onto staves of whole slots."""
    slots = cycle["slots"]
    placed = {
        task: coverage(cycle, tasks, task)
        for task in sorted(tasks)
        if any(window["task"] == task for window in cycle["windows"])
    }
    idle = idle_slots(cycle, tasks)
    per_stave = max(1, (width - labels) // SLOT_COLUMNS)

    lines = [_paint(_header(name, slots, slot_us, width), _DIM, color)]
    for first in range(0, slots, per_stave):
        last = min(first + per_stave, slots)
        lines.append("")
        lines.append(" " * labels + ruler(first, last))
        for task, held in placed.items():
            lines.append(_paint(label(task, labels) + lane(held, first, last), colours[task], color))
        lines.append(
            _paint(label("idle", labels) + lane(idle, first, last, IDLE, " "), _DIM, color)
        )
    reserved = slots - len(idle)
    lines.append(
        _paint(
            " " * labels
            + f"{reserved}/{slots} slots reserved · {(100 * reserved) // max(slots, 1)}%",
            _DIM,
            color,
        )
    )
    return "\n".join(line.rstrip() for line in lines)


def _header(name: str, slots: int, slot_us: int, width: int) -> str:
    """Return the schedule's heading, dropping the durations if it does not fit."""
    parts = [f"schedule {name}", _slots(slots), _duration(slots * slot_us)]
    parts.append(f"slot {_duration(slot_us)}")
    while len(" · ".join(parts)) > width and len(parts) > 2:
        parts.pop()
    return " · ".join(parts)


def _slots(count: int) -> str:
    return _count(count, "slot")


def _count(count: int, noun: str) -> str:
    return f"{count} {noun}" + ("" if count == 1 else "s")


def _duration(microseconds: int) -> str:
    if microseconds >= 1000000 and microseconds % 1000000 == 0:
        return f"{microseconds // 1000000} s"
    if microseconds >= 1000 and microseconds % 1000 == 0:
        return f"{microseconds // 1000} ms"
    return f"{microseconds} us"


def _paint(text: str, code: str, color: bool) -> str:
    """Colour a line, which is decoration only: the text itself never changes."""
    text = text.rstrip()
    return f"{code}{text}{_RESET}" if color and text else text
