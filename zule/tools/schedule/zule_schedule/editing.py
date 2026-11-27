"""Edits to a schedule, each returning a new schedule rather than changing one.

Keeping an edit pure makes it testable without a terminal, and leaves the
schedule before the edit intact for undo. An edit may leave a schedule the
semantic checks reject, since some edits need a gap that does not exist yet;
what refuses an invalid schedule is saving it, not making it.
"""

import re
from copy import deepcopy
from typing import Any

# The schema's rule for a task name, which the generated constants need.
_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


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


def cycles_of(schedule: Any) -> list[Any]:
    """Return every cycle in a schedule file, whichever form it uses."""
    if "cycle" in schedule:
        return [schedule["cycle"]]
    return list(schedule["schedules"].values())


def first_free_slot(cycle: Any, tasks: Any, task: str) -> int:
    """Return the first slot where a task's window fits, or 0 if none does."""
    budget = tasks.get(task, {}).get("budget_slots", 1)
    reserved = set()
    for window in cycle["windows"]:
        held = tasks.get(window["task"], {}).get("budget_slots", 1)
        reserved.update(range(window["start_slot"], window["start_slot"] + held))
    for start in range(max(cycle["slots"] - budget + 1, 1)):
        if reserved.isdisjoint(range(start, start + budget)):
            return start
    return 0


def add_window(schedule: Any, name: str, task: str, start: int | None = None) -> tuple[Any, int]:
    """Return the schedule with a window added, and the new window's index.

    Without a slot, the window goes where it first fits, so that adding one
    does not start by making an overlap.
    """
    edited = deepcopy(schedule)
    cycle = cycle_of(edited, name)
    if start is None:
        start = first_free_slot(cycle, edited["tasks"], task)
    added = {"task": task, "start_slot": start}
    cycle["windows"].append(added)
    windows = cycle["windows"]
    order = sorted(range(len(windows)), key=lambda at: (windows[at]["start_slot"], at))
    cycle["windows"] = [windows[at] for at in order]
    return edited, order.index(len(windows) - 1)


def task_name_issue(schedule: Any, name: str, renaming: str | None = None) -> str | None:
    """Return why a task cannot be called this, or None if it can."""
    if not _NAME.match(name):
        return "a task name must be a C identifier"
    for existing in schedule["tasks"]:
        if existing != renaming and existing.upper() == name.upper():
            return f"{existing!r} already takes that name's generated constant"
    return None


def add_task(schedule: Any, name: str, budget: int) -> Any:
    """Return the schedule with a task added, which no schedule yet places."""
    edited = deepcopy(schedule)
    edited["tasks"][name] = {"budget_slots": budget}
    return edited


def rename_task(schedule: Any, old: str, new: str) -> Any:
    """Return the schedule with a task renamed, and its windows following it."""
    edited = deepcopy(schedule)
    edited["tasks"] = {
        (new if name == old else name): task for name, task in edited["tasks"].items()
    }
    for cycle in cycles_of(edited):
        for window in cycle["windows"]:
            if window["task"] == old:
                window["task"] = new
    return edited


def delete_task(schedule: Any, name: str) -> Any:
    """Return the schedule without a task, and without the windows that placed it."""
    edited = deepcopy(schedule)
    del edited["tasks"][name]
    for cycle in cycles_of(edited):
        cycle["windows"] = [window for window in cycle["windows"] if window["task"] != name]
    return edited


def set_budget(schedule: Any, name: str, budget: int) -> Any:
    """Return the schedule with a task's budget changed.

    A budget belongs to the task, so this resizes its windows in every
    schedule, which is what can leave one overlapping or past its cycle.
    """
    edited = deepcopy(schedule)
    edited["tasks"][name]["budget_slots"] = budget
    return edited


def windows_of(schedule: Any, task: str) -> int:
    """Return how many windows place a task, across every schedule."""
    return sum(
        1 for cycle in cycles_of(schedule) for window in cycle["windows"] if window["task"] == task
    )

