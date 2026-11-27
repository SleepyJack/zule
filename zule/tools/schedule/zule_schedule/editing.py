"""Edits to a schedule, each returning a new schedule rather than changing one.

Keeping an edit pure makes it testable without a terminal, and leaves the
schedule before the edit intact for undo. An edit may leave a schedule the
semantic checks reject, since some edits need a gap that does not exist yet;
what refuses an invalid schedule is saving it, not making it.
"""

from copy import deepcopy
from typing import Any


def cycle_of(schedule: Any, name: str) -> Any:
    """Return a named schedule's cycle, whichever form the file uses."""
    if "cycle" in schedule:
        return schedule["cycle"]
    return schedule["schedules"][name]


def move_window(schedule: Any, name: str, index: int, slots: int) -> tuple[Any, int]:
    """Return the schedule with one window moved, and the window's new index.

    A window cannot start outside its cycle, so a move that would take it there
    returns the schedule unchanged. Windows are kept in start order, as the
    format requires, so one moved past another takes its place in the list.
    """
    cycle = cycle_of(schedule, name)
    start = cycle["windows"][index]["start_slot"] + slots
    if not 0 <= start < cycle["slots"]:
        return schedule, index

    edited = deepcopy(schedule)
    windows = cycle_of(edited, name)["windows"]
    windows[index]["start_slot"] = start
    # Sorting by position as well keeps windows that start together in order,
    # so the moved window is still found by its own index.
    order = sorted(range(len(windows)), key=lambda at: (windows[at]["start_slot"], at))
    cycle_of(edited, name)["windows"] = [windows[at] for at in order]
    return edited, order.index(index)


def delete_window(schedule: Any, name: str, index: int) -> tuple[Any, int]:
    """Return the schedule without one window, and the index to select next."""
    edited = deepcopy(schedule)
    windows = cycle_of(edited, name)["windows"]
    del windows[index]
    return edited, max(0, min(index, len(windows) - 1))
