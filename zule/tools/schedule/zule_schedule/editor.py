"""An interactive editor for a schedule file, on the grid that show draws.

The editor's state and the frame it draws are ordinary values, so both are
tested without a terminal: the curses layer only paints a frame and turns key
presses into state changes. A frame is rows of (text, style) spans, and the
curses layer decides what each style looks like.

Edits may leave a schedule the semantic checks reject, since some need a gap
that does not exist yet. What refuses an invalid schedule is writing it.
"""

import curses
from copy import deepcopy
from pathlib import Path
from typing import Any

from . import grid
from .editing import delete_window, move_window
from .formatting import format_schedule
from .validation import schedule_cycles, validate_schedule

PLAIN = ""
DIM = "dim"
SELECTED = "selected"
ERROR = "error"
TASK = "task"  # Followed by the task's index, so lanes keep their colours.

_KEYS = "←→ move  ↑↓ select  d delete  tab schedule  w write  q quit"


class Editor:
    """What the editor is showing and what it has been asked to do."""

    def __init__(self, path: str, schedule: Any) -> None:
        self.path = path
        self.schedule = schedule
        self.saved = deepcopy(schedule)
        self.schedule_index = 0
        self.window_index = 0
        self.first_slot = 0
        self.message = ""
        self.finished = False

    @property
    def modified(self) -> bool:
        return self.schedule != self.saved

    def cycles(self) -> list[tuple[str, Any, str]]:
        return schedule_cycles(self.schedule)

    def current(self) -> tuple[str, Any]:
        name, cycle, _ = self.cycles()[self.schedule_index]
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
        return grid.slots_per_view(width - grid.label_width(sorted(self.schedule["tasks"])))

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
        self.message = ""
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
        elif name == "tab":
            self.schedule_index = (self.schedule_index + 1) % len(self.cycles())
            self.window_index = 0
            self.first_slot = 0
        elif name == "w":
            self.write()
        elif name in ("q", "Q"):
            self.close(force=name == "Q")

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
    names = sorted(tasks)
    labels = grid.label_width(names)
    per_view = editor.per_view(width)
    styles = {name: f"{TASK}{index % 6}" for index, name in enumerate(names)}
    name, cycle = editor.current()
    first = editor.first_slot
    last = min(first + per_view, cycle["slots"])

    title = editor.path + (" · modified" if editor.modified else "")
    rows = [[(title, DIM)], [], [("tasks", DIM)]]
    for task in names:
        rows.append(
            [(grid.label(task, labels) + grid.bar(tasks[task]["budget_slots"]), styles[task])]
        )

    rows.append([])
    rows.append([(_heading(name, cycle, editor.schedule["slot_us"], first, last), DIM)])
    rows.append([(" " * labels + grid.ruler(first, last), DIM)])
    window = editor.selected()
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
    rows.append([(_KEYS, DIM)])
    return rows


def _heading(name: str, cycle: Any, slot_us: int, first: int, last: int) -> str:
    heading = f"schedule {name} · {cycle['slots']} slots · slot {slot_us} us"
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
    if editor.message:
        return [(editor.message, PLAIN)]
    issues = validate_schedule(editor.schedule)
    reserved = cycle["slots"] - len(idle)
    usage = f"   {reserved}/{cycle['slots']} slots reserved"
    if issues:
        return [(f"{_issues(len(issues))}: {issues[0].path}: {issues[0].message}", ERROR)]
    return [("valid", PLAIN), (usage, DIM)]


def _issues(count: int) -> str:
    return f"{count} issue" + ("" if count == 1 else "s")


def run(path: str, schedule: Any) -> int:
    """Edit a schedule in the terminal, returning the process status."""
    editor = Editor(path, schedule)
    curses.wrapper(_loop, editor)
    return 0


def _loop(screen: Any, editor: Editor) -> None:
    curses.use_default_colors()
    for pair in range(1, 7):
        curses.init_pair(pair, (curses.COLOR_CYAN, curses.COLOR_YELLOW, curses.COLOR_GREEN,
                                curses.COLOR_MAGENTA, curses.COLOR_BLUE, curses.COLOR_RED)[pair - 1], -1)
    screen.keypad(True)
    while not editor.finished:
        height, width = screen.getmaxyx()
        editor.ensure_visible(width)
        paint(screen, frame(editor, width, height), width)
        editor.key(key_name(screen.getch()))


def paint(screen: Any, rows: list[list[tuple[str, str]]], width: int, attribute: Any = None) -> None:
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
        return curses.color_pair(int(style[len(TASK):]) + 1)
    return curses.A_NORMAL


def key_name(key: int) -> str:
    """Return the editor's name for a key press."""
    return {
        curses.KEY_LEFT: "left",
        curses.KEY_RIGHT: "right",
        curses.KEY_UP: "up",
        curses.KEY_DOWN: "down",
        ord("\t"): "tab",
    }.get(key, chr(key) if 0 <= key < 0x110000 else "")
