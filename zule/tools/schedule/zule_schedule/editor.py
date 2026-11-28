"""An interactive editor for a schedule file, on the grid that show draws.

The editor draws a file the way show prints it: its tasks, which every schedule
in it shares, then each schedule below the last. One cursor moves over the
whole of it, a cell at a time, and enter acts on the cell it is on. A cell is a
task's name or budget, a schedule's name or cycle length, a lane or one slot
of it, or a row that adds a task, places one in a schedule, or adds a schedule.

A schedule draws a lane for each task it places, as show does, and a row that
reveals an empty lane for one it does not. A revealed lane is the editor's own
state rather than the file's: one left without windows is simply not there the
next time the file is opened. The keys the current
cell offers are listed at the foot of the screen, so there is no mode to
remember; the one exception is holding a window, which the screen says.

The editor's state and the frame it draws are ordinary values, so both are
tested without a terminal: the curses layer only paints a frame and turns key
presses into state changes. A frame is rows of spans, and the curses layer
decides what each style looks like.

Edits may leave a schedule the semantic checks reject, since some need a gap
that does not exist yet. What refuses an invalid schedule is writing it.
"""

import curses
from copy import deepcopy
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import grid
from .editing import (
    add_recurring,
    add_schedule,
    add_task,
    add_window,
    delete_lane,
    delete_schedule,
    delete_task,
    delete_window,
    lane_windows,
    move_window,
    placement_issue,
    rename_schedule,
    rename_task,
    schedule_name_issue,
    set_budget,
    set_slots,
    task_name_issue,
    window_at,
    window_issue,
    windows_of,
)
from .formatting import format_schedule
from .validation import schedule_cycles, validate_schedule

PLAIN = ""
DIM = "dim"
SELECTED = "selected"
HELD = "held"
BAD = "bad"  # A held window where it cannot stay.
ERROR = "error"
TASK = "task"  # Followed by the task's index, so lanes keep their colours.
BRIGHT = "bright-"  # Before another style: the cursor's slot, in its own colour.

TASK_NAME = "task-name"
TASK_BUDGET = "task-budget"
NEW_TASK = "new-task"
SCHEDULE_NAME = "schedule-name"
SCHEDULE_SLOTS = "schedule-slots"
LANE = "lane"
SLOT = "slot"
PLACE_TASK = "place-task"
NEW_SCHEDULE = "new-schedule"

_MOVE = "  ↑↓←→ move  w write  q quit"


@dataclass(frozen=True)
class Cell:
    """One thing the cursor can rest on, and enter can act upon."""

    kind: str
    schedule: int = -1
    task: str = ""
    slot: int = -1


@dataclass
class Span:
    """A run of text in a row, navigable when it carries a cell."""

    text: str
    style: str = PLAIN
    cell: Cell | None = None
    column: int = 0


@dataclass
class Prompt:
    """A line the editor is asking for, and what to do with it."""

    label: str
    action: str
    text: str = ""
    data: Any = None


@dataclass
class Editor:
    """What the editor is showing and what it has been asked to do."""

    path: str
    schedule: Any
    width: int = 80
    height: int = 24
    row: int = 0
    cell: int = 0
    column_wanted: int = 0
    holding: tuple[int, int] | None = None
    first_row: int = 0
    message: str = ""
    prompt: Prompt | None = None
    finished: bool = False
    interrupted: bool = False
    saved: Any = None
    first_slots: dict[str, int] = field(default_factory=dict)
    revealed: dict[str, set[str]] = field(default_factory=dict)

    def __post_init__(self) -> None:
        self.saved = deepcopy(self.schedule)
        self.row = _rows_with_cells(layout(self))[0]

    @property
    def modified(self) -> bool:
        return self.schedule != self.saved

    def task_names(self) -> list[str]:
        return sorted(self.schedule["tasks"])

    def cycles(self) -> list[tuple[str, Any]]:
        return [(name, cycle) for name, cycle, _ in schedule_cycles(self.schedule)]

    def budget(self, task: str) -> int:
        """Return a task's budget, or one slot for a task the file does not declare."""
        return self.schedule["tasks"].get(task, {}).get("budget_slots", 1)

    def lanes(self, name: str, cycle: Any) -> list[str]:
        """Return the tasks one schedule draws a lane for, in task order."""
        shown = {window["task"] for window in cycle["windows"]} | self.revealed.get(name, set())
        return [task for task in self.task_names() if task in shown]

    def unplaced(self, name: str, cycle: Any) -> list[str]:
        """Return the tasks that schedule has no lane for."""
        drawn = set(self.lanes(name, cycle))
        return [task for task in self.task_names() if task not in drawn]

    def reveal(self, name: str, task: str) -> None:
        """Draw an empty lane for a task, so a window can be placed in it."""
        self.revealed.setdefault(name, set()).add(task)

    def per_view(self) -> int:
        """Return how many slots a grid holds at the current width."""
        return grid.slots_per_view(self.width - grid.label_width(self.task_names()))

    def first_slot(self, name: str) -> int:
        """Return a schedule's own horizontal position, so each scrolls alone."""
        cycle = dict(self.cycles()).get(name, {"slots": 1})
        return max(0, min(self.first_slots.get(name, 0), max(0, cycle["slots"] - self.per_view())))

    def cursor(self) -> Cell | None:
        """Return the cell the cursor is on."""
        rows = layout(self)
        self._clamp(rows)
        navigable = _cells(rows[self.row])
        return rows[self.row][navigable[self.cell]].cell if navigable else None

    def resize(self, width: int, height: int) -> None:
        self.width = width
        self.height = height

    def scroll(self) -> None:
        """Scroll down the least that keeps the cursor's row on screen."""
        rows = layout(self)
        self._clamp(rows)
        body = max(self.height - 2, 1)
        self.first_row = max(0, min(self.first_row, self.row))
        if self.row >= self.first_row + body:
            self.first_row = self.row - body + 1

    def _clamp(self, rows: list[list[Span]]) -> None:
        """Keep the cursor on a cell that still exists."""
        with_cells = _rows_with_cells(rows)
        if self.row not in with_cells:
            self.row = min(with_cells, key=lambda row: abs(row - self.row))
        self.cell = max(0, min(self.cell, len(_cells(rows[self.row])) - 1))

    def key(self, name: str) -> None:
        """Apply one key press."""
        if name != "interrupt":
            self.interrupted = False
        if self.prompt is not None:
            self._answer(name)
            return
        self.message = ""
        if name == "w":
            self.write()
        elif name in ("q", "Q"):
            self.close(force=name == "Q")
        elif name == "interrupt":
            self.close(force=self.interrupted)
            self.interrupted = not self.finished
        elif name == "eof":
            # The terminal has gone, so there is nobody left to ask.
            self.finished = True
        elif self.holding is not None:
            self._hold_key(name)
        else:
            self._move_key(name)

    def _hold_key(self, name: str) -> None:
        """Keys while a window is held: it moves rather than the cursor."""
        index, window = self.holding
        schedule_name, _ = self.cycles()[index]
        if name in ("left", "right"):
            moved, new_index = move_window(
                self.schedule, schedule_name, window, -1 if name == "left" else 1
            )
            if moved is self.schedule:
                self.message = "the window cannot start outside its cycle"
                return
            self.schedule = moved
            self.holding = (index, new_index)
            self._follow()
        elif name == "d":
            self.schedule, _ = delete_window(self.schedule, schedule_name, window)
            self.holding = None
            self.message = "deleted the window"
        elif name in ("enter", "escape"):
            self.holding = None

    def _follow(self) -> None:
        """Put the cursor on the held window's first slot, scrolling to it."""
        index, window = self.holding
        name, cycle = self.cycles()[index]
        start = cycle["windows"][window]["start_slot"]
        first = self.first_slots.get(name, 0)
        if start < first:
            self.first_slots[name] = start
        elif start >= first + self.per_view():
            self.first_slots[name] = start - self.per_view() + 1
        self._put_on(Cell(SLOT, index, cycle["windows"][window]["task"], start))

    def _put_on(self, target: Cell) -> None:
        """Move the cursor to a cell, if the screen still has one like it."""
        rows = layout(self)
        for row_index, row in enumerate(rows):
            for cell_index, at in enumerate(_cells(row)):
                if row[at].cell == target:
                    self.row, self.cell = row_index, cell_index
                    self.column_wanted = row[at].column
                    return

    def _move_key(self, name: str) -> None:
        if name in ("up", "down"):
            self._move_row(1 if name == "down" else -1)
        elif name in ("left", "right"):
            self._move_cell(1 if name == "right" else -1)
        elif name == "enter":
            self._enter()
        elif name == "r":
            self._repeat()
        elif name in ("d", "D"):
            self._delete(force=name == "D")

    def _move_row(self, step: int) -> None:
        rows = layout(self)
        self._clamp(rows)
        with_cells = _rows_with_cells(rows)
        at = with_cells.index(self.row)
        self.row = with_cells[max(0, min(at + step, len(with_cells) - 1))]
        row = rows[self.row]
        navigable = _cells(row)
        self.cell = min(
            range(len(navigable)), key=lambda index: abs(row[navigable[index]].column - self.column_wanted)
        )

    def _move_cell(self, step: int) -> None:
        rows = layout(self)
        self._clamp(rows)
        row = rows[self.row]
        navigable = _cells(row)
        here = row[navigable[self.cell]].cell
        target = self.cell + step
        inside = 0 <= target < len(navigable)
        # Leaving a lane leftwards for its label is the same edge as leaving it
        # rightwards: the cycle scrolls while there is more of it that way.
        leaving = not inside or _kind_of(row[navigable[target]].cell) == LANE
        if leaving and self._scroll_across(here, step):
            return
        if inside:
            self.cell = target
            self.column_wanted = row[navigable[target]].column

    def _scroll_across(self, cell: Cell | None, step: int) -> bool:
        """At a lane's edge, bring the next slot of a long cycle into view."""
        if cell is None or cell.kind != SLOT:
            return False
        name, cycle = self.cycles()[cell.schedule]
        first = self.first_slot(name)
        if step > 0 and first + self.per_view() < cycle["slots"]:
            self.first_slots[name] = first + 1
            return True
        if step < 0 and first > 0:
            self.first_slots[name] = first - 1
            return True
        return False

    def _enter(self) -> None:
        cell = self.cursor()
        if cell is None:
            return
        if cell.kind == SLOT:
            self._enter_slot(cell)
        elif cell.kind == TASK_NAME:
            self.prompt = Prompt(f"rename {cell.task} to", "rename-task", data=cell.task)
        elif cell.kind == TASK_BUDGET:
            self.prompt = Prompt(f"budget in slots for {cell.task}", "budget", data=cell.task)
        elif cell.kind == NEW_TASK:
            self.prompt = Prompt("new task name", "new-task")
        elif cell.kind == SCHEDULE_NAME:
            name = self.cycles()[cell.schedule][0]
            self.prompt = Prompt(f"rename schedule {name} to", "rename-schedule", data=name)
        elif cell.kind == SCHEDULE_SLOTS:
            name = self.cycles()[cell.schedule][0]
            self.prompt = Prompt(f"slots in the {name} cycle", "slots", data=name)
        elif cell.kind == PLACE_TASK:
            name, cycle = self.cycles()[cell.schedule]
            choices = ", ".join(self.unplaced(name, cycle))
            self.prompt = Prompt(f"place which task ({choices})", "place-task", data=name)
        elif cell.kind == NEW_SCHEDULE:
            self.prompt = Prompt("new schedule name", "new-schedule")

    def _enter_slot(self, cell: Cell) -> None:
        """Pick up the window under the cursor, or place one where it is not."""
        name, cycle = self.cycles()[cell.schedule]
        index = window_at(cycle, self.schedule["tasks"], cell.task, cell.slot)
        if index is not None:
            self.holding = (cell.schedule, index)
            return
        issue = placement_issue(cycle, self.schedule["tasks"], cell.task, cell.slot)
        if issue:
            self.message = issue
            return
        self.schedule, index = add_window(self.schedule, name, cell.task, cell.slot)
        self.message = f"placed {cell.task} at slot {cell.slot}"
        self.holding = (cell.schedule, index)
        self._follow()
        self.holding = None

    def _repeat(self) -> None:
        """Ask for the period of a series of windows anchored where the cursor is."""
        cell = self.cursor()
        if cell is None or cell.kind != SLOT:
            return
        name, cycle = self.cycles()[cell.schedule]
        index = window_at(cycle, self.schedule["tasks"], cell.task, cell.slot)
        start = cycle["windows"][index]["start_slot"] if index is not None else cell.slot
        self.prompt = Prompt(
            f"a {cell.task} window every how many slots, from slot {start}",
            "repeat",
            data=(name, cell.task, start),
        )

    def _delete(self, force: bool) -> None:
        cell = self.cursor()
        if cell is None:
            return
        if cell.kind == SLOT:
            name, cycle = self.cycles()[cell.schedule]
            index = window_at(cycle, self.schedule["tasks"], cell.task, cell.slot)
            if index is None:
                return
            self.schedule, _ = delete_window(self.schedule, name, index)
            self.message = f"deleted a {cell.task} window"
        elif cell.kind == LANE:
            self._delete_lane(cell.schedule, cell.task, force)
        elif cell.kind in (TASK_NAME, TASK_BUDGET):
            self._delete_task(cell.task, force)
        elif cell.kind in (SCHEDULE_NAME, SCHEDULE_SLOTS):
            self._delete_schedule(cell.schedule, force)

    def _delete_lane(self, index: int, task: str, force: bool) -> None:
        """Clear a task's windows from one schedule, or hide a lane it has none in."""
        name, cycle = self.cycles()[index]
        placed = lane_windows(cycle, task)
        if not placed:
            self.revealed.get(name, set()).discard(task)
            self.message = f"hid the empty {task} lane"
            return
        if placed > 1 and not force:
            self.message = f"{task} has {_count(placed, 'window')} here: D deletes them"
            return
        self.schedule = delete_lane(self.schedule, name, task)
        self.revealed.get(name, set()).discard(task)
        self.message = f"cleared {task} from {name}"

    def _delete_task(self, task: str, force: bool) -> None:
        if len(self.task_names()) == 1:
            self.message = "a schedule file needs at least one task"
            return
        placed = windows_of(self.schedule, task)
        if placed and not force:
            self.message = f"{task} has {_count(placed, 'window')}: D deletes it and them"
            return
        self.schedule = delete_task(self.schedule, task)
        self.message = f"deleted {task}"

    def _delete_schedule(self, index: int, force: bool) -> None:
        cycles = self.cycles()
        if len(cycles) == 1:
            self.message = "a schedule file needs at least one schedule"
            return
        name, cycle = cycles[index]
        if cycle["windows"] and not force:
            self.message = f"{name} has {_count(len(cycle['windows']), 'window')}: D deletes it"
            return
        self.schedule = delete_schedule(self.schedule, name)
        self.message = f"deleted schedule {name}"

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
        issue = _commit_issue(self, prompt, text)
        if issue:
            self.message = issue
            return
        if prompt.action == "new-task":
            self.prompt = Prompt(f"budget in slots for {text}", "new-task-budget", data=text)
            return
        if prompt.action == "new-schedule":
            name, cycle = self.cycles()[max(_schedule_of(self.cursor()), 0)]
            self.schedule = add_schedule(
                self.schedule, text, cycle["slots"], self.task_names()[0]
            )
            self.message = f"added schedule {text} with one window"
        elif prompt.action == "repeat":
            name, task, start = prompt.data
            self.schedule, placed, skipped = add_recurring(
                self.schedule, name, task, start, int(text)
            )
            self.message = _repeated(task, start, placed, skipped)
        elif prompt.action == "place-task":
            self._place_task(prompt.data, text)
        elif prompt.action == "rename-task":
            self.schedule = rename_task(self.schedule, prompt.data, text)
            for tasks in self.revealed.values():
                if prompt.data in tasks:
                    tasks.discard(prompt.data)
                    tasks.add(text)
        elif prompt.action == "rename-schedule":
            self.schedule = rename_schedule(self.schedule, prompt.data, text)
            for state in (self.revealed, self.first_slots):
                if prompt.data in state:
                    state[text] = state.pop(prompt.data)
        elif prompt.action == "new-task-budget":
            self.schedule = add_task(self.schedule, prompt.data, int(text))
            self.message = f"added {prompt.data}: place task gives it a lane"
        elif prompt.action == "budget":
            self.schedule = set_budget(self.schedule, prompt.data, int(text))
        elif prompt.action == "slots":
            self.schedule = set_slots(self.schedule, prompt.data, int(text))
        self.prompt = None

    def _place_task(self, schedule: str, task: str) -> None:
        """Reveal an empty lane, and put the cursor in it ready to place a window."""
        self.reveal(schedule, task)
        index = [name for name, _ in self.cycles()].index(schedule)
        self._put_on(Cell(SLOT, index, task, self.first_slot(schedule)))
        self.message = f"{task} has a lane in {schedule}: enter places a window"

    def write(self) -> None:
        """Write the schedule, unless it still has issues to fix."""
        issues = validate_schedule(self.schedule)
        if issues:
            self.message = f"not written: fix {_count(len(issues), 'issue')} first"
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


def _repeated(task: str, start: int, placed: list[int], skipped: list[tuple[int, str]]) -> str:
    """Say what a series of windows managed, since a period need not divide a cycle."""
    if not placed and not skipped:
        return f"no further {task} window fits from slot {start}"
    done = f"placed {_count(len(placed), 'window')}" if placed else "placed nothing"
    if not skipped:
        return done
    slots = ", ".join(str(slot) for slot, _ in skipped[:4])
    more = "" if len(skipped) <= 4 else ", …"
    return f"{done}; no room at {'slot' if len(skipped) == 1 else 'slots'} {slots}{more}"


def _commit_issue(editor: Editor, prompt: Prompt, text: str) -> str | None:
    """Return why a finished line cannot be used, or None if it can."""
    if prompt.action in ("new-task", "rename-task"):
        return task_name_issue(editor.schedule, text, prompt.data)
    if prompt.action in ("new-schedule", "rename-schedule"):
        return schedule_name_issue(editor.schedule, text, prompt.data)
    if prompt.action == "place-task":
        cycle = dict(editor.cycles())[prompt.data]
        if text not in editor.unplaced(prompt.data, cycle):
            return f"{prompt.data} has no unplaced task called {text!r}"
        return None
    nouns = {"slots": "cycle", "repeat": "period"}
    if not text.isdigit() or int(text) < 1:
        return f"a {nouns.get(prompt.action, 'budget')} is a whole number of slots, at least one"
    if prompt.action == "repeat":
        _, task, _ = prompt.data
        budget = editor.budget(task)
        if int(text) < budget:
            return f"{task} needs {_count(budget, 'slot')}, so its windows cannot be that close"
    return None


def _kind_of(cell: Cell | None) -> str:
    return cell.kind if cell is not None else ""


def _schedule_of(cell: Cell | None) -> int:
    return cell.schedule if cell is not None else -1


def layout(editor: Editor) -> list[list[Span]]:
    """Return every row of the editor's content, with the cells it can rest on."""
    tasks = editor.schedule["tasks"]
    names = editor.task_names()
    labels = grid.label_width(names)
    styles = {name: f"{TASK}{index % 6}" for index, name in enumerate(names)}
    width = editor.width

    rows = [
        _row(Span(editor.path + (" · modified" if editor.modified else ""), DIM)),
        _row(),
        _row(Span("tasks", DIM)),
    ]
    counts = {task: _count(tasks[task]["budget_slots"], "slot") for task in names}
    column = max([len(count) for count in counts.values()] + [0]) + 2
    room = max(width - labels - column - 1, 1)
    for task in names:
        budget = str(tasks[task]["budget_slots"])
        rows.append(
            _row(
                Span(" " * grid.MARGIN),
                Span(task, PLAIN, Cell(TASK_NAME, task=task)),
                Span(" " * (labels - grid.MARGIN - len(task))),
                Span(budget, PLAIN, Cell(TASK_BUDGET, task=task)),
                Span(counts[task][len(budget) :].ljust(column), DIM),
                Span(grid.bar(tasks[task]["budget_slots"])[:room], styles[task]),
            )
        )
    rows.append(_row(Span(" " * grid.MARGIN), Span("+ new task", DIM, Cell(NEW_TASK))))

    for index, (name, cycle) in enumerate(editor.cycles()):
        first = editor.first_slot(name)
        last = min(first + editor.per_view(), cycle["slots"])
        rows.append(_row())
        rows.append(_heading(index, name, cycle, first, last))
        rows.append(_row(Span(" " * labels + grid.ruler(first, last), DIM)))
        for task in editor.lanes(name, cycle):
            rows.append(_lane(editor, index, task, cycle, first, last, labels, styles[task]))
        idle = grid.idle_slots(cycle, tasks)
        rows.append(
            _row(Span(grid.label("idle", labels) + grid.lane(idle, first, last, grid.IDLE, " "), DIM))
        )
        if editor.unplaced(name, cycle):
            rows.append(
                _row(
                    Span(" " * grid.MARGIN),
                    Span("+ place task", DIM, Cell(PLACE_TASK, schedule=index)),
                )
            )
    rows.append(_row())
    rows.append(_row(Span(" " * grid.MARGIN), Span("+ new schedule", DIM, Cell(NEW_SCHEDULE))))
    return rows


def _row(*spans: Span) -> list[Span]:
    column = 0
    for span in spans:
        span.column = column
        column += len(span.text)
    return list(spans)


def _heading(index: int, name: str, cycle: Any, first: int, last: int) -> list[Span]:
    showing = f" · showing {first}-{last - 1}" if last - first < cycle["slots"] else ""
    return _row(
        Span("schedule ", DIM),
        Span(name, PLAIN, Cell(SCHEDULE_NAME, schedule=index)),
        Span(" · ", DIM),
        Span(str(cycle["slots"]), PLAIN, Cell(SCHEDULE_SLOTS, schedule=index)),
        Span(" slots" + showing, DIM),
    )


def _lane(
    editor: Editor,
    index: int,
    task: str,
    cycle: Any,
    first: int,
    last: int,
    labels: int,
    style: str,
) -> list[Span]:
    """Return a task's lane in one schedule, one cell per slot it can hold."""
    held = grid.coverage(cycle, editor.schedule["tasks"], task)
    drawn = grid.lane(held, first, last)
    spans = [
        Span(" " * grid.MARGIN),
        Span(task, DIM if not held else PLAIN, Cell(LANE, index, task)),
        Span(" " * (labels - grid.MARGIN - len(task))),
    ]
    for position, slot in enumerate(range(first, last)):
        text = drawn[position * grid.SLOT_COLUMNS : (position + 1) * grid.SLOT_COLUMNS]
        spans.append(Span(text, style if slot in held else DIM, Cell(SLOT, index, task, slot)))
    return _row(*spans)


def _cells(row: list[Span]) -> list[int]:
    return [index for index, span in enumerate(row) if span.cell is not None]


def _rows_with_cells(rows: list[list[Span]]) -> list[int]:
    return [index for index, row in enumerate(rows) if _cells(row)]


def frame(editor: Editor) -> list[list[tuple[str, str]]]:
    """Return the rows to paint, each a list of (text, style) spans."""
    rows = layout(editor)
    editor._clamp(rows)
    here = _cells(rows[editor.row])[editor.cell] if _cells(rows[editor.row]) else -1
    holding = HELD if _held_issue(editor) is None else BAD
    painted = []
    for row_index, row in enumerate(rows):
        spans = []
        for span_index, span in enumerate(row):
            if _is_held(editor, span.cell):
                # The whole window is drawn as one piece, so the cursor's own
                # slot is not picked out within it.
                style = holding
            elif row_index == editor.row and span_index == here:
                # A slot keeps its own colour, brightened: reversing a block
                # sets the lane's colour behind it, which shows as stripes.
                style = BRIGHT + span.style if _is_slot(span.cell) else SELECTED
            else:
                style = span.style
            spans.append((span.text, style))
        painted.append(spans)

    body = max(editor.height - 2, 1)
    painted = painted[editor.first_row : editor.first_row + body]
    painted = painted + [[]] * max(0, body - len(painted))
    painted.append(_status(editor))
    painted.append([(_keys(editor), DIM)])
    return painted


def _is_slot(cell: Cell | None) -> bool:
    """Say whether the terminal's own cursor is what marks this cell."""
    return cell is not None and cell.kind == SLOT


def _is_held(editor: Editor, cell: Cell | None) -> bool:
    if editor.holding is None or cell is None or cell.kind != SLOT:
        return False
    index, window = editor.holding
    name, cycle = editor.cycles()[index]
    if cell.schedule != index or window >= len(cycle["windows"]):
        return False
    start = cycle["windows"][window]["start_slot"]
    task = cycle["windows"][window]["task"]
    return cell.task == task and start <= cell.slot < start + editor.budget(task)


def _held_issue(editor: Editor) -> str | None:
    """Return why a held window cannot stay where it is, or None if it can."""
    if editor.holding is None:
        return None
    index, window = editor.holding
    _, cycle = editor.cycles()[index]
    if window >= len(cycle["windows"]):
        return None
    return window_issue(cycle, editor.schedule["tasks"], window)


def _status(editor: Editor) -> list[tuple[str, str]]:
    if editor.prompt is not None:
        return [
            (f"{editor.prompt.label}: ", DIM),
            (editor.prompt.text, PLAIN),
            ("▌", SELECTED),
            (f"   {editor.message}" if editor.message else "", ERROR),
        ]
    if editor.message:
        return [(editor.message, PLAIN)]
    held = _held_issue(editor)
    if held:
        return [(held, ERROR)]
    issues = validate_schedule(editor.schedule)
    if issues:
        return [(f"{_count(len(issues), 'issue')}: {issues[0].path}: {issues[0].message}", ERROR)]
    return [("valid", PLAIN)]


def caret(editor: Editor) -> tuple[int, int] | None:
    """Return where the terminal's own cursor belongs, or None to hide it.

    A prompt is typed into, so the cursor belongs after what has been typed.
    Otherwise it rests on the cell the cursor is on, which is what marks a
    slot: reversing one would set the lane's colour as the cell's background
    and redraw the block over it, which a terminal shows as stripes.
    """
    if editor.prompt is not None:
        column = len(f"{editor.prompt.label}: ") + len(editor.prompt.text)
        return max(editor.height - 2, 0), max(0, min(column, editor.width - 2))
    rows = layout(editor)
    editor._clamp(rows)
    navigable = _cells(rows[editor.row])
    if not navigable:
        return None
    row = editor.row - editor.first_row
    if not 0 <= row < max(editor.height - 2, 1):
        return None
    column = rows[editor.row][navigable[editor.cell]].column
    return row, max(0, min(column, editor.width - 2))


def _keys(editor: Editor) -> str:
    """Return what the cursor's cell offers, so no key has to be remembered."""
    if editor.prompt is not None:
        return "  enter accept  escape cancel"
    if editor.holding is not None:
        return "  ←→ move  d delete  enter done"
    cell = editor.cursor()
    if cell is None:
        return _MOVE
    if cell.kind == LANE:
        name, cycle = editor.cycles()[cell.schedule]
        if lane_windows(cycle, cell.task):
            return f"  d clear {cell.task} from {name}" + _MOVE
        return "  d hide the empty lane" + _MOVE
    if cell.kind == SLOT:
        name, cycle = editor.cycles()[cell.schedule]
        if window_at(cycle, editor.schedule["tasks"], cell.task, cell.slot) is not None:
            return "  enter pick up  d delete  r repeat" + _MOVE
        return f"  enter place {cell.task} here  r repeat" + _MOVE
    offers = {
        TASK_NAME: "  enter rename  d delete",
        TASK_BUDGET: "  enter set budget  d delete",
        NEW_TASK: "  enter add a task",
        SCHEDULE_NAME: "  enter rename  d delete",
        SCHEDULE_SLOTS: "  enter set cycle length  d delete",
        PLACE_TASK: "  enter give a task a lane here",
        NEW_SCHEDULE: "  enter add a schedule",
    }
    return offers[cell.kind] + _MOVE


def _count(count: int, noun: str) -> str:
    return f"{count} {noun}" + ("" if count == 1 else "s")


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
    curses.init_pair(_HELD_PAIR, curses.COLOR_WHITE, -1)
    curses.init_pair(_BAD_PAIR, curses.COLOR_RED, -1)
    screen.keypad(True)
    while not editor.finished:
        height, width = screen.getmaxyx()
        editor.resize(width, height)
        editor.scroll()
        paint(screen, frame(editor), width, spot=caret(editor))
        try:
            pressed = screen.getch()
        except KeyboardInterrupt:
            pressed = _INTERRUPT
        editor.key(key_name(pressed))


def paint(
    screen: Any,
    rows: list[list[tuple[str, str]]],
    width: int,
    attribute: Any = None,
    spot: tuple[int, int] | None = None,
) -> None:
    """Draw a frame, leaving the last column clear so that writing never wraps.

    What a style looks like is the terminal's business, so tests pass their own.
    The cursor rests where spot says, and is hidden when there is nowhere for
    it to be, so that it never sits on a row the editor did not put it in.
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
    try:
        curses.curs_set(1 if spot else 0)
    except curses.error:
        pass  # A terminal that cannot do without its cursor keeps it.
    if spot:
        screen.move(*spot)
    screen.refresh()


def _attribute(style: str) -> int:
    if style.startswith(BRIGHT):
        under = style[len(BRIGHT) :]
        # Bold over dim is undefined between terminals, so a slot with nothing
        # in it brightens from the plain foreground instead.
        return curses.A_BOLD | (0 if under == DIM else _attribute(under))
    if style == DIM:
        return curses.A_DIM
    if style == SELECTED:
        return curses.A_REVERSE
    # A held window is drawn in colour rather than reversed: reversing a full
    # block paints the cell in the background colour, which turns the window
    # dark instead of lifting it, and leaves little of it to colour red.
    if style == HELD:
        return curses.A_BOLD | curses.color_pair(_HELD_PAIR)
    if style == BAD:
        return curses.A_BOLD | curses.color_pair(_BAD_PAIR)
    if style == ERROR:
        return curses.A_BOLD
    if style.startswith(TASK):
        return curses.color_pair(int(style[len(TASK) :]) + 1)
    return curses.A_NORMAL


_HELD_PAIR = 7
_BAD_PAIR = 8
_INTERRUPT = -2


def key_name(key: int) -> str:
    """Return the editor's name for a key press."""
    named = {
        -1: "eof",
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
    }
    if key in named:
        return named[key]
    return chr(key) if 0 <= key < 0x110000 else ""
