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


def schedule_names(schedule: Any) -> list[str]:
    """Return the file's schedule names, in the order of their generated IDs."""
    if "cycle" in schedule:
        return ["main"]
    return sorted(schedule["schedules"])


def schedule_name_issue(schedule: Any, name: str, renaming: str | None = None) -> str | None:
    """Return why a schedule cannot be called this, or None if it can."""
    if not _NAME.match(name):
        return "a schedule name must be a C identifier"
    for existing in schedule_names(schedule):
        if existing != renaming and existing.upper() == name.upper():
            return f"{existing!r} already takes that name's generated constant"
    return None


def _as_named(schedule: Any) -> Any:
    """Return the schedule in the named form, which a single cycle calls main.

    The two forms mean the same thing, so converting one is faithful; it is
    what lets a second schedule be added to a file that has only a cycle.
    """
    if "cycle" not in schedule:
        return schedule
    converted = {}
    for key, value in schedule.items():
        if key == "cycle":
            converted["schedules"] = {"main": value}
        else:
            converted[key] = value
    return converted


def add_schedule(schedule: Any, name: str, slots: int, task: str) -> Any:
    """Return the schedule with another added, holding one window to begin with.

    A cycle needs a window, so the new schedule starts with one for the given
    task at slot zero.
    """
    edited = _as_named(deepcopy(schedule))
    edited["schedules"][name] = {
        "slots": slots,
        "windows": [{"task": task, "start_slot": 0}],
    }
    return edited


def rename_schedule(schedule: Any, old: str, new: str) -> Any:
    """Return the schedule with one renamed, keeping the others in place."""
    edited = _as_named(deepcopy(schedule))
    edited["schedules"] = {
        (new if name == old else name): cycle for name, cycle in edited["schedules"].items()
    }
    return edited


def delete_schedule(schedule: Any, name: str) -> Any:
    """Return the schedule without one, which the caller checks is not the last."""
    edited = _as_named(deepcopy(schedule))
    del edited["schedules"][name]
    return edited


def set_slots(schedule: Any, name: str, slots: int) -> Any:
    """Return the schedule with a cycle's length changed.

    A shorter cycle can leave windows past its end, which the checks report.
    """
    edited = deepcopy(schedule)
    cycle_of(edited, name)["slots"] = slots
    return edited


def placement_issue(cycle: Any, tasks: Any, task: str, start: int) -> str | None:
    """Return why a task's window cannot start at a slot, or None if it can."""
    budget = tasks.get(task, {}).get("budget_slots", 1)
    if start + budget > cycle["slots"]:
        return f"{task} needs {budget} slots and the cycle ends at {cycle['slots']}"
    reserved = set()
    for window in cycle["windows"]:
        held = tasks.get(window["task"], {}).get("budget_slots", 1)
        reserved.update(range(window["start_slot"], window["start_slot"] + held))
    if not reserved.isdisjoint(range(start, start + budget)):
        return f"{task} needs {budget} slots and they are not free here"
    return None


def window_at(cycle: Any, tasks: Any, task: str, slot: int) -> int | None:
    """Return the index of the task's window covering a slot, if one does."""
    budget = tasks.get(task, {}).get("budget_slots", 1)
    for index, window in enumerate(cycle["windows"]):
        if window["task"] == task and window["start_slot"] <= slot < window["start_slot"] + budget:
            return index
    return None


def window_issue(cycle: Any, tasks: Any, index: int) -> str | None:
    """Return why a window does not fit where it is, or None if it does.

    This is what placement_issue asks of a slot, asked instead of a window
    already in the cycle, so that one being moved can say as it goes.
    """
    window = cycle["windows"][index]
    start = window["start_slot"]
    budget = tasks.get(window["task"], {}).get("budget_slots", 1)
    if start + budget > cycle["slots"]:
        return f"{window['task']} would run past the end of the cycle"
    for other, placed in enumerate(cycle["windows"]):
        if other == index:
            continue
        held = tasks.get(placed["task"], {}).get("budget_slots", 1)
        if start < placed["start_slot"] + held and placed["start_slot"] < start + budget:
            return f"{window['task']} would overlap {placed['task']}"
    return None

