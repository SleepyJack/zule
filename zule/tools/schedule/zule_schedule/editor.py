"""An interactive editor for a schedule file, on the grid that show draws.

The editor has the two areas show prints: the file's tasks, which every
schedule in it shares, and the schedule below, where their windows are placed.
Focus moves between them, so a window is added by picking its task up in one
area and placing it in the other.

The editor's state and the frame it draws are ordinary values, so both are
tested without a terminal: the curses layer only paints a frame and turns key
presses into state changes. A frame is rows of (text, style) spans, and the
curses layer decides what each style looks like.

Edits may leave a schedule the semantic checks reject, since some need a gap
that does not exist yet. What refuses an invalid schedule is writing it.
"""

import curses
from copy import deepcopy
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from . import grid
from .editing import (
    add_task,
    add_window,
    delete_task,
    delete_window,
    move_window,
    rename_task,
    set_budget,
    task_name_issue,
    windows_of,
)
from .formatting import format_schedule
from .validation import schedule_cycles, validate_schedule

PLAIN = ""
DIM = "dim"
SELECTED = "selected"
ERROR = "error"
TASK = "task"  # Followed by the task's index, so lanes keep their colours.

TASKS = "tasks"
SCHEDULE = "schedule"

_KEYS = {
    TASKS: "↑↓ task  a add window  n new  r rename  b budget  d delete"
    "  tab schedule  w write  q quit",
    SCHEDULE: "↑↓ window  ←→ move  d delete  s next schedule  tab tasks  w write  q quit",
}


@dataclass
class Prompt:
    """A line the editor is asking for, and what to do with it."""

    label: str
    action: str
    text: str = ""
    data: str = ""


class Editor:
    """What the editor is showing and what it has been asked to do."""

    def __init__(self, path: str, schedule: Any) -> None:
        self.path = path
        self.schedule = schedule
        self.saved = deepcopy(schedule)
        self.focus = SCHEDULE
        self.schedule_index = 0
        self.window_index = 0
        self.task_index = 0
        self.first_slot = 0
        self.message = ""
        self.prompt: Prompt | None = None
        self.finished = False
        self.interrupted = False

    @property
    def modified(self) -> bool:
        return self.schedule != self.saved

    def task_names(self) -> list[str]:
        return sorted(self.schedule["tasks"])

    def task(self) -> str | None:
        names = self.task_names()
        return names[min(self.task_index, len(names) - 1)] if names else None

    def current(self) -> tuple[str, Any]:
        name, cycle, _ = schedule_cycles(self.schedule)[self.schedule_index]
        return name, cycle

    def windows(self) -> list[Any]:
        return self.current()[1]["windows"]

    def selected(self) -> Any:
        windows = self.windows()
        return windows[self.window_index] if windows else None

    def budget(self, task: str) -> int:
        """Return a task's budget, or one slot for a task the file does not declare."""
        return self.schedule["tasks"].get(task, {}).get("budget_slots", 1)

    def per_view(self, width: int) -> int:
        """Return how many slots the view holds, which the frame also uses."""
        return grid.slots_per_view(width - grid.label_width(self.task_names()))

    def ensure_visible(self, width: int) -> None:
        """Scroll the view the least that shows the selected window."""
        per_view = self.per_view(width)
        _, cycle = self.current()
        self.first_slot = max(0, min(self.first_slot, max(0, cycle["slots"] - per_view)))
        window = self.selected()
        if window is None:
            return
        start = window["start_slot"]
        end = min(start + self.budget(window["task"]), cycle["slots"])
        if start < self.first_slot:
            self.first_slot = start
        elif end > self.first_slot + per_view:
            self.first_slot = max(0, end - per_view)

    def key(self, name: str) -> None:
        """Apply one key press."""
        if name != "interrupt":
            self.interrupted = False
        if self.prompt is not None:
            self._answer(name)
            return
        self.message = ""
        if name == "tab":
            self.focus = TASKS if self.focus == SCHEDULE else SCHEDULE
        elif name == "w":
            self.write()
        elif name in ("q", "Q"):
            self.close(force=name == "Q")
        elif name == "interrupt":
            self.close(force=self.interrupted)
            self.interrupted = not self.finished
        elif self.focus == TASKS:
            self._task_key(name)
        else:
            self._schedule_key(name)

    def _task_key(self, name: str) -> None:
        names = self.task_names()
        task = self.task()
        if name in ("up", "down") and names:
            step = 1 if name == "down" else -1
            self.task_index = (self.task_index + step) % len(names)
        elif name == "a" and task is not None:
            self.schedule, self.window_index = add_window(self.schedule, self.current()[0], task)
            self.focus = SCHEDULE
            self.message = f"added a window for {task}"
        elif name == "n":
            self.prompt = Prompt("new task name", "name")
        elif name == "r" and task is not None:
            self.prompt = Prompt(f"rename {task} to", "rename", data=task)
        elif name == "b" and task is not None:
            self.prompt = Prompt(f"budget in slots for {task}", "budget", data=task)
        elif name in ("d", "D") and task is not None:
            self._delete_task(task, force=name == "D")

    def _delete_task(self, task: str, force: bool) -> None:
        if len(self.task_names()) == 1:
            self.message = "a schedule file needs at least one task"
            return
        placed = windows_of(self.schedule, task)
        if placed and not force:
            self.message = f"{task} has {_windows(placed)}: D deletes it and them"
            return
        self.schedule = delete_task(self.schedule, task)
        self.task_index = min(self.task_index, len(self.task_names()) - 1)
        self.window_index = 0
        self.message = f"deleted {task}"

    def _schedule_key(self, name: str) -> None:
        windows = self.windows()
        schedule_name = self.current()[0]
        if name in ("left", "right") and windows:
            moved, self.window_index = move_window(
                self.schedule, schedule_name, self.window_index, -1 if name == "left" else 1
            )
            if moved is self.schedule:
                self.message = "the window cannot start outside its cycle"
            self.schedule = moved
        elif name in ("up", "down") and windows:
            step = 1 if name == "down" else -1
            self.window_index = (self.window_index + step) % len(windows)
        elif name == "d" and windows:
            self.schedule, self.window_index = delete_window(
                self.schedule, schedule_name, self.window_index
            )
        elif name == "s":
            self.schedule_index = (self.schedule_index + 1) % len(schedule_cycles(self.schedule))
            self.window_index = 0
            self.first_slot = 0

    def _answer(self, name: str) -> None:
        """Collect one key of the line the editor asked for."""
        prompt = self.prompt
        if name == "escape":
            self.prompt = None
            self.message = "cancelled"
        elif name == "backspace":
            prompt.text = prompt.text[:-1]
        elif name == "enter":
            self._commit(prompt)
        elif len(name) == 1 and name.isprintable():
            prompt.text += name

    def _commit(self, prompt: Prompt) -> None:
        """Act on a finished line, keeping the prompt open if it cannot be used."""
        text = prompt.text.strip()
        self.message = ""
        if prompt.action in ("name", "rename"):
            issue = task_name_issue(self.schedule, text, prompt.data or None)
            if issue:
                self.message = issue
                return
            if prompt.action == "name":
                self.prompt = Prompt(f"budget in slots for {text}", "new-budget", data=text)
                return
            self.schedule = rename_task(self.schedule, prompt.data, text)
            self.task_index = self.task_names().index(text)
        else:
            if not text.isdigit() or int(text) < 1:
                self.message = "a budget is a whole number of slots, at least one"
                return
            if prompt.action == "new-budget":
                self.schedule = add_task(self.schedule, prompt.data, int(text))
                self.task_index = self.task_names().index(prompt.data)
                self.message = f"added {prompt.data}: a gives it a window"
            else:
                self.schedule = set_budget(self.schedule, prompt.data, int(text))
        self.prompt = None

    def write(self) -> None:
        """Write the schedule, unless it still has issues to fix."""
        issues = validate_schedule(self.schedule)
        if issues:
            self.message = f"not written: fix {_issues(len(issues))} first"
            return
        try:
            Path(self.path).write_text(format_schedule(self.schedule), encoding="utf-8")
        except OSError as error:
            self.message = f"not written: {error}"
            return
        self.saved = deepcopy(self.schedule)
        self.message = f"written to {self.path}"

    def close(self, force: bool) -> None:
        if self.modified and not force:
            self.message = "unsaved changes: w to write, Q to quit anyway"
            return
        self.finished = True


def frame(editor: Editor, width: int, height: int) -> list[list[tuple[str, str]]]:
    """Return the rows to paint, each a list of (text, style) spans."""
    tasks = editor.schedule["tasks"]
    names = editor.task_names()
    labels = grid.label_width(names)
    styles = {name: f"{TASK}{index % 6}" for index, name in enumerate(names)}
    name, cycle = editor.current()
    first = editor.first_slot
    last = min(first + editor.per_view(width), cycle["slots"])

    rows = [[(editor.path + (" · modified" if editor.modified else ""), DIM)], []]
    rows.append([(_marker(editor, TASKS) + "tasks", DIM)])
    for task in names:
        bar = grid.bar(tasks[task]["budget_slots"])[: max(width - labels - 1, 1)]
        chosen = editor.focus == TASKS and task == editor.task()
        rows.append(
            [(grid.label(task, labels), SELECTED if chosen else PLAIN), (bar, styles[task])]
        )

    rows.append([])
    rows.append([(_marker(editor, SCHEDULE) + _heading(name, cycle, first, last), DIM)])
    rows.append([(" " * labels + grid.ruler(first, last), DIM)])
    window = editor.selected() if editor.focus == SCHEDULE else None
    for task in names:
        held = grid.coverage(cycle, tasks, task)
        if not held:
            continue
        text = grid.label(task, labels) + grid.lane(held, first, last)
        highlight = None
        if window is not None and window["task"] == task:
            highlight = _columns(editor, window, first, last, labels)
        rows.append(_spans(text, highlight, styles[task]))
    idle = grid.idle_slots(cycle, tasks)
    rows.append([(grid.label("idle", labels) + grid.lane(idle, first, last, grid.IDLE, " "), DIM)])

    body = height - 2
    rows = rows[:body] + [[]] * max(0, body - len(rows))
    rows.append(_status(editor, cycle, idle))
    rows.append([(_KEYS[editor.focus], DIM)])
    return rows


def _marker(editor: Editor, area: str) -> str:
    """Mark the area with focus, so that the keys below apply to it."""
    return "▸ " if editor.focus == area else "  "


def _heading(name: str, cycle: Any, first: int, last: int) -> str:
    heading = f"schedule {name} · {cycle['slots']} slots"
    if last - first < cycle["slots"]:
        heading += f" · showing {first}-{last - 1}"
    return heading


def _columns(editor: Editor, window: Any, first: int, last: int, labels: int) -> tuple[int, int]:
    """Return the columns the selected window occupies in the view."""
    start = window["start_slot"]
    end = start + editor.budget(window["task"])
    left = labels + (max(start, first) - first) * grid.SLOT_COLUMNS
    right = labels + (min(end, last) - first) * grid.SLOT_COLUMNS
    if end <= last:
        right -= 1  # The drawn window leaves its last column blank.
    return left, max(right, left)


def _spans(text: str, highlight: tuple[int, int] | None, style: str) -> list[tuple[str, str]]:
    if highlight is None:
        return [(text, style)]
    left, right = highlight
    parts = [(text[:left], style), (text[left:right], SELECTED), (text[right:], style)]
    return [(part, part_style) for part, part_style in parts if part]


def _status(editor: Editor, cycle: Any, idle: dict[int, list[int]]) -> list[tuple[str, str]]:
    if editor.prompt is not None:
        return [
            (f"{editor.prompt.label}: ", PLAIN),
            (editor.prompt.text + "▌", SELECTED),
            (f"   {editor.message}" if editor.message else "", ERROR),
        ]
    if editor.message:
        return [(editor.message, PLAIN)]
    issues = validate_schedule(editor.schedule)
    if issues:
        return [(f"{_issues(len(issues))}: {issues[0].path}: {issues[0].message}", ERROR)]
    reserved = cycle["slots"] - len(idle)
    return [("valid", PLAIN), (f"   {reserved}/{cycle['slots']} slots reserved", DIM)]


def _issues(count: int) -> str:
    return f"{count} issue" + ("" if count == 1 else "s")


def _windows(count: int) -> str:
    return f"{count} window" + ("" if count == 1 else "s")


def run(path: str, schedule: Any) -> int:
    """Edit a schedule in the terminal, returning the process status."""
    editor = Editor(path, schedule)
    try:
        curses.wrapper(_loop, editor)
    except KeyboardInterrupt:
        return 130
    return 0


def _loop(screen: Any, editor: Editor) -> None:
    curses.use_default_colors()
    colours = (
        curses.COLOR_CYAN,
        curses.COLOR_YELLOW,
        curses.COLOR_GREEN,
        curses.COLOR_MAGENTA,
        curses.COLOR_BLUE,
        curses.COLOR_RED,
    )
    for pair, colour in enumerate(colours, start=1):
        curses.init_pair(pair, colour, -1)
    screen.keypad(True)
    while not editor.finished:
        height, width = screen.getmaxyx()
        editor.ensure_visible(width)
        paint(screen, frame(editor, width, height), width)
        try:
            pressed = screen.getch()
        except KeyboardInterrupt:
            pressed = _INTERRUPT
        editor.key(key_name(pressed))


def paint(
    screen: Any, rows: list[list[tuple[str, str]]], width: int, attribute: Any = None
) -> None:
    """Draw a frame, leaving the last column clear so that writing never wraps.

    What a style looks like is the terminal's business, so tests pass their own.
    """
    attribute = attribute or _attribute
    screen.erase()
    for row, spans in enumerate(rows):
        column = 0
        for text, style in spans:
            text = text[: max(0, width - column - 1)]
            if text:
                screen.addstr(row, column, text, attribute(style))
            column += len(text)
    screen.refresh()


def _attribute(style: str) -> int:
    if style == DIM:
        return curses.A_DIM
    if style == SELECTED:
        return curses.A_REVERSE
    if style == ERROR:
        return curses.A_BOLD
    if style.startswith(TASK):
        return curses.color_pair(int(style[len(TASK) :]) + 1)
    return curses.A_NORMAL


_INTERRUPT = -2


def key_name(key: int) -> str:
    """Return the editor's name for a key press."""
    named = {
        _INTERRUPT: "interrupt",
        3: "interrupt",  # Ctrl-C, where the terminal passes it through.
        curses.KEY_LEFT: "left",
        curses.KEY_RIGHT: "right",
        curses.KEY_UP: "up",
        curses.KEY_DOWN: "down",
        curses.KEY_BACKSPACE: "backspace",
        curses.KEY_ENTER: "enter",
        8: "backspace",
        10: "enter",
        13: "enter",
        27: "escape",
        127: "backspace",
        ord("\t"): "tab",
    }
    if key in named:
        return named[key]
    return chr(key) if 0 <= key < 0x110000 else ""
