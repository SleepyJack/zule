"""The slot grid a schedule is drawn on, shared by the renderer and the editor.

A slot is always the same number of columns, so a task's budget and its windows
are drawn to the same scale and compare directly. The last column of a window is
left blank, so that windows which touch stay apart.
"""

from typing import Any

# The development environment is UTF-8; see the tool's README.
WINDOW = "█"  # Full block, for a task's budget and its reserved windows.
IDLE = "░"  # Light shade, for slots no window reserves.
CONFLICT = "▒"  # Medium shade, where one task's own windows overlap.
SLOT_MARK = "·"  # Middle dot, marking each slot of an unreserved lane.

SLOT_COLUMNS = 5
MARGIN = 2


def label_width(tasks: Any) -> int:
    """Return the width of the lane labels, which every block shares."""
    return MARGIN + max([len(task) for task in tasks] + [len("idle")]) + 2


def label(text: str, width: int) -> str:
    return (" " * MARGIN + text).ljust(width)


def slots_per_view(columns: int) -> int:
    """Return how many whole slots fit in the columns left for the grid."""
    return max(1, columns // SLOT_COLUMNS)


def coverage(cycle: Any, tasks: Any, task: str) -> dict[int, list[int]]:
    """Return each slot a task's windows reserve, with the windows reserving it.

    A slot held by more than one window is an overlap the semantic checks
    report. Slots past the cycle's end are left out, since nothing draws them.
    """
    held: dict[int, list[int]] = {}
    budget = tasks[task]["budget_slots"]
    for index, window in enumerate(cycle["windows"]):
        if window["task"] != task:
            continue
        start = window["start_slot"]
        for slot in range(max(start, 0), min(start + budget, cycle["slots"])):
            held.setdefault(slot, []).append(index)
    return held


def idle_slots(cycle: Any, tasks: Any) -> dict[int, list[int]]:
    """Return the slots of a cycle that no window reserves."""
    reserved = {
        slot
        for task in tasks
        for slot in coverage(cycle, tasks, task)
    }
    return {slot: [] for slot in range(cycle["slots"]) if slot not in reserved}


def lane(
    held: dict[int, list[int]],
    first: int,
    last: int,
    char: str = WINDOW,
    empty: str = SLOT_MARK,
) -> str:
    """Draw one lane's slots, leaving the last column of each run blank."""
    columns = []
    for slot in range(first, last):
        if slot not in held:
            columns.append(empty.ljust(SLOT_COLUMNS))
            continue
        fill = CONFLICT if len(held[slot]) > 1 else char
        ends = slot + 1 not in held or held[slot + 1] != held[slot]
        columns.append(fill * (SLOT_COLUMNS - 1) + (" " if ends else fill))
    return "".join(columns)


def ruler(first: int, last: int) -> str:
    """Return the slot numbers of one view, at the start of each slot."""
    return "".join(str(slot).ljust(SLOT_COLUMNS) for slot in range(first, last))


def bar(budget: int) -> str:
    """Return a task's budget drawn as one of its windows is."""
    return WINDOW * max(budget * SLOT_COLUMNS - 1, 1)
