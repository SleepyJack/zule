"""Render a schedule as a slot grid, with one lane per task.

The renderer is a pure function of the schedule, so it is testable without a
terminal, and it tolerates schedules that fail the semantic checks: windows that
overlap or reach past the cycle still render, since seeing them is the point.
It requires the structure the schema defines; see schema_issues().
"""

from typing import Any

from .validation import schedule_cycles

# The development environment is UTF-8; see the tool's README.
_WINDOW = "█"  # Full block, for a task's reserved window.
_IDLE = "░"  # Light shade, for slots no window reserves.
_CONFLICT = "▒"  # Medium shade, where one task's own windows overlap.
_SLOT_MARK = "·"  # Middle dot, marking each slot in an empty lane.

_RESET = "\033[0m"
_DIM = "\033[2m"
_LANE_COLOURS = ("\033[36m", "\033[33m", "\033[32m", "\033[35m", "\033[34m", "\033[31m")

# Below this many columns the grid is unreadable, so annotations are dropped
# before it shrinks further.
_MIN_GRID_COLUMNS = 16
# Wider slots do not read any better, and waste space in long cycles.
_MAX_SLOT_COLUMNS = 5


def render_schedule(schedule: Any, *, width: int = 80, color: bool = False) -> str:
    """Return every schedule in the file as a slot grid, in schedule ID order."""
    tasks = schedule["tasks"]
    blocks = [
        _render_cycle(name, cycle, tasks, schedule["slot_us"], width, color)
        for name, cycle, _ in schedule_cycles(schedule)
    ]
    return "\n\n".join(blocks)


def _render_cycle(
    name: str, cycle: Any, tasks: Any, slot_us: int, width: int, color: bool
) -> str:
    slots = cycle["slots"]
    windows = [window for window in cycle["windows"] if window["task"] in tasks]
    names = sorted(tasks)
    placed = {window["task"] for window in windows}

    label_width = max([len(task) for task in names] + [len("idle")]) + 2
    notes = {task: _note(tasks[task]["budget_slots"], task in placed) for task in names}
    note_width = max([len(note) for note in notes.values()] + [0]) + 2

    grid = width - label_width - note_width
    if grid < _MIN_GRID_COLUMNS:
        note_width = 0
        grid = max(_MIN_GRID_COLUMNS, width - label_width)
    scale = _Scale(slots, grid)

    lines = [
        _paint(_header(name, slots, slot_us, scale.exact, width), _DIM, color),
        "",
        " " * label_width + _ruler(scale),
    ]
    for index, task in enumerate(names):
        lane = [" "] * scale.columns
        if scale.slot_columns > 1:
            # Below two columns a slot there is no room for the mark, and a run
            # of them would read as a filled window.
            for slot in range(slots):
                _place(lane, scale.column(slot), _SLOT_MARK)
        for window in windows:
            if window["task"] == task:
                _fill(lane, scale, window["start_slot"], tasks[task]["budget_slots"], _WINDOW)
        line = task.ljust(label_width) + "".join(lane)
        if note_width:
            line = line.rstrip().ljust(label_width + scale.columns + 2) + notes[task]
        lines.append(_paint(line, _LANE_COLOURS[index % len(_LANE_COLOURS)], color))

    reserved = _reserved_slots(windows, tasks, slots)
    idle = [" "] * scale.columns
    for slot in range(slots):
        if slot not in reserved:
            _fill(idle, scale, slot, 1, _IDLE, conflicts=False)
    lines.append(_paint("idle".ljust(label_width) + "".join(idle), _DIM, color))
    used = len(reserved)
    lines.append(
        _paint(
            " " * label_width + f"{used}/{slots} slots reserved"
            f" · {(100 * used) // slots if slots else 0}%",
            _DIM,
            color,
        )
    )
    return "\n".join(line.rstrip() for line in lines)


def _header(name: str, slots: int, slot_us: int, exact: bool, width: int) -> str:
    """Return the schedule's heading, dropping the durations if it does not fit.

    Each part is paired with whether it may be dropped.
    """
    parts = [
        (f"schedule {name}", False),
        (_slots(slots), False),
        (_duration(slots * slot_us), True),
        (f"slot {_duration(slot_us)}", True),
    ]
    if not exact:
        parts.append(("scaled to fit", False))

    def joined() -> str:
        return " · ".join(text for text, _ in parts)

    while len(joined()) > width and any(droppable for _, droppable in parts):
        last = max(index for index, (_, droppable) in enumerate(parts) if droppable)
        del parts[last]
    return joined()


class _Scale:
    """Map slots to grid columns, compressing proportionally when they do not fit."""

    def __init__(self, slots: int, grid: int) -> None:
        self.slots = max(slots, 1)
        self.exact = self.slots <= grid
        if self.exact:
            self.slot_columns = min(_MAX_SLOT_COLUMNS, grid // self.slots)
            self.columns = self.slots * self.slot_columns
        else:
            self.slot_columns = 0
            self.columns = grid

    def column(self, slot: int) -> int:
        """Return the first column of a slot, clamped to the grid."""
        return min(slot, self.slots) * self.columns // self.slots


def _reserved_slots(windows: Any, tasks: Any, slots: int) -> set[int]:
    reserved = set()
    for window in windows:
        start = window["start_slot"]
        for slot in range(start, min(start + tasks[window["task"]]["budget_slots"], slots)):
            reserved.add(slot)
    return reserved


def _fill(
    lane: list[str], scale: _Scale, start: int, budget: int, char: str, conflicts: bool = True
) -> None:
    """Fill a window's columns, leaving a gap so that touching windows stay apart.

    With conflicts, columns another window of the same task already filled are
    marked instead, since they are an overlap that the semantic checks report.
    Idle slots pass False, since a compressed grid puts several in one column.
    """
    first = scale.column(start)
    last = scale.column(start + budget)
    if scale.slot_columns > 1:
        last -= 1
    for column in range(first, max(last, first + 1)):
        if 0 <= column < len(lane):
            taken = conflicts and lane[column] in (char, _CONFLICT)
            lane[column] = _CONFLICT if taken else char


def _place(line: list[str], column: int, text: str) -> None:
    for offset, char in enumerate(text):
        if 0 <= column + offset < len(line):
            line[column + offset] = char


def _ruler(scale: _Scale) -> str:
    """Return the slot numbers, at the widest spacing the labels fit."""
    width = len(str(scale.slots - 1))
    step = 1
    while step < scale.slots and scale.column(step) < width + 1:
        step += 1
    line = [" "] * scale.columns
    for slot in range(0, scale.slots, step):
        label = str(slot)
        if scale.column(slot) + len(label) <= scale.columns:
            _place(line, scale.column(slot), label)
    return "".join(line)


def _note(budget: int, placed: bool) -> str:
    return _slots(budget) if placed else _slots(budget) + " (no window)"


def _slots(count: int) -> str:
    return f"{count} slot" + ("" if count == 1 else "s")


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
