"""Render a schedule file as a task list and a slot grid.

The tasks and their budgets come first, since a file's tasks are shared by every
schedule in it, and each schedule follows as a grid of slots with one lane per
task it places. A slot is always the same width, so a task's budget and its
windows are drawn to the same scale; a cycle too long for the terminal continues
on the next stave.

Rendering is a pure function of the schedule, so it is testable without a
terminal, and it tolerates schedules that fail the semantic checks: windows that
overlap or reach past the cycle still render, since seeing them is the point.
It requires the structure the schema defines; see schema_issues().
"""

from typing import Any

from .validation import schedule_cycles

# The development environment is UTF-8; see the tool's README.
_WINDOW = "█"  # Full block, for a task's budget and its reserved windows.
_IDLE = "░"  # Light shade, for slots no window reserves.
_CONFLICT = "▒"  # Medium shade, where one task's own windows overlap.
_SLOT_MARK = "·"  # Middle dot, marking each slot of an unreserved lane.

_RESET = "\033[0m"
_DIM = "\033[2m"
_LANE_COLOURS = ("\033[36m", "\033[33m", "\033[32m", "\033[35m", "\033[34m", "\033[31m")

# Columns per slot, shared by every lane so that budgets and windows compare
# directly. The last column of a window is left blank, so that windows which
# touch stay apart.
_SLOT_COLUMNS = 5
_MARGIN = 2


def render_schedule(schedule: Any, *, width: int = 80, color: bool = False) -> str:
    """Return the file's tasks, then each schedule's grid in schedule ID order."""
    tasks = schedule["tasks"]
    names = sorted(tasks)
    label_width = _MARGIN + max([len(name) for name in names] + [len("idle")]) + 2
    colours = {name: _LANE_COLOURS[index % len(_LANE_COLOURS)] for index, name in enumerate(names)}
    blocks = [_task_block(tasks, names, label_width, width, colours, color)]
    blocks.extend(
        _cycle_block(name, cycle, tasks, schedule["slot_us"], label_width, width, colours, color)
        for name, cycle, _ in schedule_cycles(schedule)
    )
    return "\n\n".join(blocks)


def _task_block(
    tasks: Any, names: list[str], label_width: int, width: int, colours: Any, color: bool
) -> str:
    """Return the task list, each task's budget drawn as a window would be."""
    bar_columns = max([_bar_width(tasks[name]["budget_slots"]) for name in names] + [0])
    bar_columns = min(bar_columns, max(width - label_width, _SLOT_COLUMNS))
    lines = [_paint(f"tasks · {_count(len(names), 'task')}", _DIM, color)]
    for name in names:
        bar = _WINDOW * min(_bar_width(tasks[name]["budget_slots"]), bar_columns)
        line = _label(name, label_width) + bar.ljust(bar_columns + 2)
        lines.append(_paint(line + _slots(tasks[name]["budget_slots"]), colours[name], color))
    return "\n".join(line.rstrip() for line in lines)


def _cycle_block(
    name: str,
    cycle: Any,
    tasks: Any,
    slot_us: int,
    label_width: int,
    width: int,
    colours: Any,
    color: bool,
) -> str:
    """Return one schedule's grid, wrapped onto staves of whole slots."""
    slots = cycle["slots"]
    coverage = {
        task: _coverage(cycle, tasks, task)
        for task in sorted(tasks)
        if any(window["task"] == task for window in cycle["windows"])
    }
    reserved = {slot for slots_held in coverage.values() for slot in slots_held}
    idle = {slot: [] for slot in range(slots) if slot not in reserved}
    per_stave = max(1, (width - label_width) // _SLOT_COLUMNS)

    lines = [_paint(_header(name, slots, slot_us, width), _DIM, color)]
    for first in range(0, slots, per_stave):
        last = min(first + per_stave, slots)
        lines.append("")
        lines.append(" " * label_width + _ruler(first, last))
        for task, held in coverage.items():
            lines.append(
                _paint(_label(task, label_width) + _lane(held, first, last), colours[task], color)
            )
        lines.append(
            _paint(
                _label("idle", label_width) + _lane(idle, first, last, _IDLE, " "), _DIM, color
            )
        )
    lines.append(
        _paint(
            " " * label_width
            + f"{len(reserved)}/{slots} slots reserved · {(100 * len(reserved)) // max(slots, 1)}%",
            _DIM,
            color,
        )
    )
    return "\n".join(line.rstrip() for line in lines)


def _coverage(cycle: Any, tasks: Any, task: str) -> dict[int, list[int]]:
    """Return each slot a task's windows reserve, with the windows reserving it.

    A slot held by more than one window is an overlap the semantic checks
    report. Slots past the cycle's end are left out, since no stave shows them.
    """
    held: dict[int, list[int]] = {}
    budget = tasks[task]["budget_slots"]
    for index, window in enumerate(cycle["windows"]):
        if window["task"] != task:
            continue
        start = window["start_slot"]
        for slot in range(start, min(start + budget, cycle["slots"])):
            held.setdefault(slot, []).append(index)
    return held


def _lane(
    held: dict[int, list[int]], first: int, last: int, char: str = _WINDOW, empty: str = _SLOT_MARK
) -> str:
    """Draw one lane's slots, leaving the last column of each run blank."""
    columns = []
    for slot in range(first, last):
        if slot not in held:
            columns.append(empty.ljust(_SLOT_COLUMNS))
            continue
        fill = _CONFLICT if len(held[slot]) > 1 else char
        ends = slot + 1 not in held or held[slot + 1] != held[slot]
        columns.append(fill * (_SLOT_COLUMNS - 1) + (" " if ends else fill))
    return "".join(columns)


def _ruler(first: int, last: int) -> str:
    """Return the slot numbers of one stave, at the start of each slot."""
    return "".join(str(slot).ljust(_SLOT_COLUMNS) for slot in range(first, last))


def _label(text: str, label_width: int) -> str:
    return (" " * _MARGIN + text).ljust(label_width)


def _bar_width(budget: int) -> int:
    return max(budget * _SLOT_COLUMNS - 1, 1)


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
