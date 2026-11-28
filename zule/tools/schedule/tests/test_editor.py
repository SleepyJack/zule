import curses
import tempfile
import unittest
from pathlib import Path

from zule_schedule.editing import (
    add_recurring,
    add_schedule,
    delete_window,
    move_window,
    placement_issue,
    schedule_names,
    window_at,
)
from zule_schedule.editor import (
    BAD,
    DIM,
    PLAIN,
    Cell,
    HELD,
    LANE,
    NEW_SCHEDULE,
    NEW_TASK,
    PLACE_TASK,
    SCHEDULE_NAME,
    SCHEDULE_SLOTS,
    SELECTED,
    SLOT,
    TASK_BUDGET,
    TASK_NAME,
    Editor,
    caret,
    frame,
    key_name,
    paint,
)
from zule_schedule import grid
from zule_schedule.schedule_io import load_schedule
from zule_schedule.validation import validate_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"


def editor(name="valid/control-loop.json", path="schedule.json", width=76, height=30):
    return Editor(path, load_schedule(FIXTURES / name), width=width, height=height)


def press(state, *keys):
    for key in keys:
        state.key(key)


def typed(state, line):
    press(state, *line, "enter")


def lines(state):
    state.scroll()
    return ["".join(text for text, _ in row) for row in frame(state)]


def keys(state):
    return lines(state)[-1]


def status(state):
    return lines(state)[-2]


# The second cell of a row is reached by moving across it.
ACROSS = {SCHEDULE_SLOTS: SCHEDULE_NAME, TASK_BUDGET: TASK_NAME, SLOT: LANE}


def to_cell(state, kind, task=None, schedule=None):
    """Move the cursor to the first cell, then down and across to this kind."""
    first = ACROSS.get(kind, kind)
    for _ in range(40):
        state.key("up")
    for _ in range(4):
        state.key("left")
    for _ in range(40):
        cell = state.cursor()
        wanted = (task is None or cell.task == task) and (
            schedule is None or cell.schedule == schedule
        )
        if cell.kind == kind and wanted:
            return cell
        if cell.kind == first and wanted:
            state.key("right")
            return state.cursor()
        state.key("down")
    raise AssertionError(f"no {kind} cell for {task}")


def to_slot(state, task, slot):
    to_cell(state, SLOT, task)
    while state.cursor().slot < slot:
        state.key("right")
    while state.cursor().slot > slot:
        state.key("left")
    return state.cursor()


class EditingTests(unittest.TestCase):
    """The edits themselves, which never change the schedule given to them."""

    def setUp(self):
        self.schedule = load_schedule(FIXTURES / "valid/control-loop.json")
        self.before = load_schedule(FIXTURES / "valid/control-loop.json")

    def test_a_series_reports_what_it_skipped_and_changes_nothing_given(self):
        edited, placed, skipped = add_recurring(self.schedule, "main", "sense", 0, 4)
        self.assertEqual(self.schedule, self.before)
        # control holds slots 2 to 4, so the series skips that one and goes on.
        self.assertEqual(placed, [8])
        self.assertEqual([slot for slot, _ in skipped], [4])
        self.assertEqual(
            [w["start_slot"] for w in edited["cycle"]["windows"] if w["task"] == "sense"],
            [0, 8],
        )

    def test_moving_leaves_the_original_schedule_alone(self):
        moved, _ = move_window(self.schedule, "main", 0, 1)
        self.assertEqual(self.schedule, self.before)
        self.assertEqual(moved["cycle"]["windows"][0]["start_slot"], 1)

    def test_a_window_moved_past_another_is_reordered(self):
        """The format requires increasing start slots, so the list is kept sorted."""
        moved, index = move_window(self.schedule, "main", 0, 3)
        self.assertEqual(moved["cycle"]["windows"][index]["task"], "sense")
        starts = [window["start_slot"] for window in moved["cycle"]["windows"]]
        self.assertEqual(starts, sorted(starts))

    def test_a_window_cannot_start_outside_its_cycle(self):
        self.assertIs(move_window(self.schedule, "main", 0, -1)[0], self.schedule)
        self.assertIs(move_window(self.schedule, "main", 2, 5)[0], self.schedule)

    def test_deleting_leaves_the_original_schedule_alone(self):
        deleted, index = delete_window(self.schedule, "main", 2)
        self.assertEqual(self.schedule, self.before)
        self.assertEqual(len(deleted["cycle"]["windows"]), 2)
        self.assertEqual(index, 1)

    def test_a_slot_is_free_only_where_the_whole_budget_fits(self):
        cycle, tasks = self.schedule["cycle"], self.schedule["tasks"]
        self.assertIsNone(placement_issue(cycle, tasks, "control", 7))
        self.assertIn("cycle ends", placement_issue(cycle, tasks, "control", 8))
        self.assertIn("not free", placement_issue(cycle, tasks, "control", 0))

    def test_a_window_is_found_by_the_slots_it_covers(self):
        cycle, tasks = self.schedule["cycle"], self.schedule["tasks"]
        self.assertEqual(window_at(cycle, tasks, "control", 4), 1)
        self.assertIsNone(window_at(cycle, tasks, "control", 5))
        self.assertIsNone(window_at(cycle, tasks, "sense", 4))

    def test_adding_a_schedule_turns_one_cycle_into_named_schedules(self):
        """The two forms mean the same, so a file with a cycle can gain a second."""
        added = add_schedule(self.schedule, "backup", 6, "sense")
        self.assertEqual(schedule_names(added), ["backup", "main"])
        self.assertEqual(validate_schedule(added), [])
        self.assertEqual(list(added), ["$schema", "slot_us", "tasks", "schedules"])
        self.assertEqual(self.schedule, self.before)


class CursorTests(unittest.TestCase):
    def test_the_cursor_starts_on_the_first_task(self):
        self.assertEqual(editor().cursor().kind, TASK_NAME)

    def test_down_reaches_every_kind_of_cell_in_turn(self):
        state = editor()
        seen = [state.cursor().kind]
        for _ in range(12):
            state.key("down")
            seen.append(state.cursor().kind)
        self.assertEqual(
            [kind for index, kind in enumerate(seen) if index == 0 or kind != seen[index - 1]],
            [TASK_NAME, NEW_TASK, SCHEDULE_NAME, LANE, NEW_SCHEDULE],
        )

    def test_the_cursor_stops_at_the_ends(self):
        state = editor()
        press(state, "up", "up")
        self.assertEqual(state.cursor().kind, TASK_NAME)
        for _ in range(30):
            state.key("down")
        self.assertEqual(state.cursor().kind, NEW_SCHEDULE)

    def test_a_row_is_crossed_cell_by_cell(self):
        state = editor()
        self.assertEqual(state.cursor().kind, TASK_NAME)
        state.key("right")
        self.assertEqual(state.cursor().kind, TASK_BUDGET)
        state.key("right")
        self.assertEqual(state.cursor().kind, TASK_BUDGET)
        state.key("left")
        self.assertEqual(state.cursor().kind, TASK_NAME)

    def test_a_schedule_heading_holds_its_name_and_its_length(self):
        state = editor()
        to_cell(state, SCHEDULE_NAME)
        state.key("right")
        self.assertEqual(state.cursor().kind, SCHEDULE_SLOTS)

    def test_moving_down_keeps_the_column_where_it_can(self):
        state = editor()
        to_slot(state, "control", 6)
        state.key("down")
        self.assertEqual(state.cursor().slot, 6)
        self.assertEqual(state.cursor().task, "output")

    def test_a_long_cycle_scrolls_at_the_edge_of_the_lane(self):
        schedule = {
            "slot_us": 1000,
            "tasks": {"sense": {"budget_slots": 1}},
            "cycle": {"slots": 40, "windows": [{"task": "sense", "start_slot": 0}]},
        }
        state = Editor("schedule.json", schedule, width=76, height=30)
        to_cell(state, SLOT)
        for _ in range(39):
            state.key("right")
        self.assertEqual(state.cursor().slot, 39)
        self.assertGreater(state.first_slot("main"), 0)
        # Coming back, the cursor keeps to the lane, scrolling the view with it.
        for slot in range(38, -1, -1):
            state.key("left")
            self.assertEqual(state.cursor().slot, slot)
        self.assertEqual(state.first_slot("main"), 0)
        # Only once slot 0 is reached does left leave the lane for its label.
        state.key("left")
        self.assertEqual(state.cursor().kind, LANE)


class EnterTests(unittest.TestCase):
    def test_enter_on_an_empty_slot_places_a_window_there(self):
        state = editor()
        to_slot(state, "sense", 7)
        state.key("enter")
        self.assertIn("placed sense at slot 7", state.message)
        self.assertIn({"task": "sense", "start_slot": 7}, state.cycles()[0][1]["windows"])

    def test_enter_on_a_slot_with_no_room_says_why(self):
        state = editor()
        to_slot(state, "control", 8)
        state.key("enter")
        self.assertIn("the cycle ends at 10", state.message)
        self.assertEqual(len(state.cycles()[0][1]["windows"]), 3)

    def test_enter_on_a_window_picks_it_up(self):
        state = editor()
        to_slot(state, "control", 3)
        state.key("enter")
        self.assertIsNotNone(state.holding)
        self.assertIn("←→ move", keys(state))

    def test_enter_on_a_task_asks_for_a_new_name(self):
        state = editor()
        state.key("enter")
        self.assertIn("rename control", state.prompt.label)
        typed(state, "regulate")
        self.assertIn("regulate", state.schedule["tasks"])
        self.assertIn("regulate", [w["task"] for w in state.cycles()[0][1]["windows"]])

    def test_enter_on_a_budget_asks_for_slots(self):
        state = editor()
        state.key("right")
        state.key("enter")
        typed(state, "4")
        self.assertEqual(state.schedule["tasks"]["control"]["budget_slots"], 4)

    def test_enter_on_the_new_task_row_adds_one(self):
        state = editor()
        to_cell(state, NEW_TASK)
        state.key("enter")
        typed(state, "logger")
        self.assertIn("budget", state.prompt.label)
        typed(state, "2")
        self.assertEqual(state.schedule["tasks"]["logger"], {"budget_slots": 2})

    def test_enter_on_a_schedule_name_renames_it(self):
        state = editor("valid/modes.json")
        to_cell(state, SCHEDULE_NAME)
        state.key("enter")
        typed(state, "running")
        self.assertIn("running", schedule_names(state.schedule))

    def test_enter_on_the_cycle_length_changes_it(self):
        state = editor()
        to_cell(state, SCHEDULE_SLOTS)
        state.key("enter")
        typed(state, "14")
        self.assertEqual(state.cycles()[0][1]["slots"], 14)

    def test_enter_on_the_new_schedule_row_adds_one(self):
        state = editor()
        to_cell(state, NEW_SCHEDULE)
        state.key("enter")
        typed(state, "backup")
        self.assertEqual(schedule_names(state.schedule), ["backup", "main"])
        self.assertEqual(validate_schedule(state.schedule), [])

    def test_names_are_checked_as_the_generated_constants_need(self):
        state = editor()
        state.key("enter")
        typed(state, "2bad")
        self.assertIn("C identifier", state.message)
        self.assertIsNotNone(state.prompt)
        press(state, *["backspace"] * 4)
        typed(state, "SENSE")
        self.assertIn("generated constant", state.message)

    def test_a_length_is_a_whole_number_of_slots(self):
        state = editor()
        to_cell(state, SCHEDULE_SLOTS)
        state.key("enter")
        typed(state, "none")
        self.assertIn("whole number", state.message)
        self.assertIsNotNone(state.prompt)

    def test_escape_abandons_a_prompt(self):
        state = editor()
        press(state, "enter", "x", "escape")
        self.assertIsNone(state.prompt)
        self.assertFalse(state.modified)


class HoldingTests(unittest.TestCase):
    def held(self, state):
        return [row for row in frame(state) if any(style == HELD for _, style in row)]

    def test_a_held_window_moves_and_the_cursor_follows(self):
        state = editor()
        to_slot(state, "sense", 0)
        press(state, "enter", "right")
        self.assertEqual(state.cycles()[0][1]["windows"][0]["start_slot"], 1)
        self.assertEqual(state.cursor().slot, 1)

    def test_a_held_window_cannot_leave_its_cycle(self):
        state = editor()
        to_slot(state, "sense", 0)
        press(state, "enter", "left")
        self.assertIn("outside its cycle", state.message)
        self.assertEqual(state.cycles()[0][1]["windows"][0]["start_slot"], 0)

    def styled(self, state, style):
        return [
            "".join(text for text, at in row if at == style)
            for row in frame(state)
            if any(at == style for _, at in row)
        ]

    def test_a_held_window_is_drawn_whole_without_the_cursor_in_it(self):
        state = editor()
        to_slot(state, "sense", 0)
        # A slot is marked by the terminal's own cursor, not by its style.
        self.assertEqual(self.styled(state, SELECTED), [])
        state.key("enter")
        # Two slots of sense, one column of the second left blank as ever.
        self.assertEqual(self.styled(state, HELD), ["█████████ "])
        self.assertEqual(self.styled(state, SELECTED), [])

    def test_a_held_window_is_marked_where_it_cannot_stay(self):
        state = editor()
        to_slot(state, "sense", 0)
        press(state, "enter", "right")
        self.assertEqual(self.styled(state, BAD), ["█████████ "])
        self.assertEqual(self.styled(state, HELD), [])
        state.key("left")
        self.assertEqual(self.styled(state, BAD), [])
        self.assertEqual(self.styled(state, HELD), ["█████████ "])

    def test_enter_puts_a_held_window_down(self):
        state = editor()
        to_slot(state, "sense", 0)
        press(state, "enter", "enter")
        self.assertIsNone(state.holding)
        press(state, "enter", "escape")
        self.assertIsNone(state.holding)

    def test_a_held_window_can_be_deleted(self):
        state = editor()
        to_slot(state, "sense", 0)
        press(state, "enter", "d")
        self.assertIsNone(state.holding)
        self.assertEqual([w["task"] for w in state.cycles()[0][1]["windows"]], ["control", "output"])


def repeating(slots=12, windows=(("sense", 0), ("control", 4))):
    """An editor on a cycle with room for a series, and something in its way."""
    schedule = {
        "slot_us": 100,
        "tasks": {"sense": {"budget_slots": 2}, "control": {"budget_slots": 3}},
        "cycle": {
            "slots": slots,
            "windows": [{"task": task, "start_slot": slot} for task, slot in windows],
        },
    }
    return Editor("schedule.json", schedule, width=76, height=30)


def starts(state, task):
    _, cycle = state.cycles()[0]
    return [w["start_slot"] for w in cycle["windows"] if w["task"] == task]


class PromptTests(unittest.TestCase):
    """What a prompt shows of itself, since it is typed into blind otherwise."""

    def test_the_typed_line_is_drawn_as_text_with_the_cursor_after_it(self):
        state = editor()
        to_cell(state, NEW_TASK)
        press(state, "enter", "l", "o", "g")
        spans = frame(state)[-2]
        self.assertEqual([text for text, _ in spans][:3], ["new task name: ", "log", "▌"])
        self.assertEqual([style for _, style in spans][:3], [DIM, PLAIN, SELECTED])
        self.assertEqual(caret(state), (state.height - 2, len("new task name: ") + 3))

    def test_outside_a_prompt_the_cursor_rests_on_the_cell(self):
        """A slot is marked by the cursor, since reversing a block shows stripes."""
        state = editor()
        self.assertEqual(caret(state), (3, grid.MARGIN))
        to_slot(state, "sense", 1)
        row, column = caret(state)
        drawn = lines(state)[row]
        # The cursor sits on the first column of the slot it is on, which here
        # is the second slot of a window, so a block rather than a slot mark.
        self.assertEqual(column, grid.label_width(state.task_names()) + grid.SLOT_COLUMNS)
        self.assertEqual(drawn[column], grid.WINDOW)

    def test_the_cursor_is_not_placed_off_the_screen(self):
        state = editor(height=6)
        to_cell(state, NEW_SCHEDULE)
        state.first_row = 0
        self.assertIsNone(caret(state))

    def test_the_cursor_stays_on_the_screen_of_a_narrow_terminal(self):
        state = editor(width=20)
        to_cell(state, NEW_TASK)
        press(state, "enter", *"a-long-task-name")
        row, column = caret(state)
        self.assertEqual(column, state.width - 2)

    def test_a_refused_line_keeps_what_was_typed_to_correct(self):
        state = editor()
        to_cell(state, TASK_BUDGET)
        press(state, "enter", "0", "enter")
        self.assertIn("at least one", state.message)
        self.assertEqual(state.prompt.text, "0")
        press(state, "backspace", "2", "enter")
        self.assertEqual(state.schedule["tasks"]["control"]["budget_slots"], 2)


class RepeatTests(unittest.TestCase):
    """A window every n slots, anchored where the cursor is."""

    def test_r_places_a_window_every_period_while_they_fit(self):
        state = repeating()
        to_slot(state, "sense", 0)
        self.assertIn("r repeat", keys(state))
        state.key("r")
        self.assertIn("every how many slots, from slot 0", status(state))
        typed(state, "4")
        self.assertEqual(starts(state, "sense"), [0, 8])
        self.assertEqual(state.message, "placed 1 window; no room at slot 4")

    def test_the_anchor_is_the_window_start_not_the_cursor_slot(self):
        state = repeating()
        to_slot(state, "sense", 1)
        state.key("r")
        self.assertIn("from slot 0", status(state))
        typed(state, "8")
        # Counted from slot 0, not from the slot the cursor was in.
        self.assertEqual(starts(state, "sense"), [0, 8])

    def test_an_empty_slot_anchors_the_series_on_itself(self):
        state = repeating(windows=(("sense", 0),))
        to_slot(state, "sense", 3)
        state.key("r")
        typed(state, "3")
        self.assertEqual(starts(state, "sense"), [0, 3, 6, 9])

    def test_a_period_shorter_than_the_budget_is_refused(self):
        state = repeating()
        to_slot(state, "sense", 0)
        state.key("r")
        typed(state, "1")
        self.assertIn("cannot be that close", state.message)
        self.assertIsNotNone(state.prompt)
        self.assertEqual(starts(state, "sense"), [0])

    def test_a_period_must_be_a_number_of_slots(self):
        state = repeating()
        to_slot(state, "sense", 0)
        state.key("r")
        typed(state, "x")
        self.assertIn("a period is a whole number of slots", state.message)

    def test_a_series_with_no_room_left_says_so(self):
        state = repeating()
        to_slot(state, "sense", 0)
        state.key("r")
        typed(state, "20")
        self.assertEqual(state.message, "no further sense window fits from slot 0")
        self.assertEqual(starts(state, "sense"), [0])

    def test_r_does_nothing_off_the_grid(self):
        state = repeating()
        to_cell(state, NEW_TASK)
        state.key("r")
        self.assertIsNone(state.prompt)


class LaneTests(unittest.TestCase):
    """Lanes a schedule draws, and the row that reveals one it does not."""

    def test_place_task_reveals_an_empty_lane_and_waits_there(self):
        state = editor("valid/modes.json")
        to_cell(state, PLACE_TASK)
        self.assertIn("give a task a lane", keys(state))
        state.key("enter")
        self.assertIn("control, sense", status(state))
        typed(state, "sense")
        self.assertEqual(state.cursor(), Cell(SLOT, 1, "sense", 0))
        self.assertIn("sense has a lane in startup", state.message)
        # The lane is the editor's own state, so the file is untouched.
        self.assertFalse(state.modified)
        self.assertTrue(any(line.startswith("  sense") for line in lines(state)[-12:]))

    def test_an_empty_lane_takes_a_window_the_ordinary_way(self):
        state = editor("valid/modes.json")
        to_cell(state, PLACE_TASK)
        press(state, "enter")
        typed(state, "sense")
        press(state, "right", "enter")
        _, cycle = state.cycles()[1]
        self.assertEqual(
            [(w["task"], w["start_slot"]) for w in cycle["windows"]],
            [("sense", 1), ("mode_manager", 4)],
        )

    def test_place_task_refuses_a_task_that_is_not_waiting(self):
        state = editor("valid/modes.json")
        to_cell(state, PLACE_TASK)
        state.key("enter")
        typed(state, "mode_manager")
        self.assertIn("no unplaced task", state.message)
        self.assertIsNotNone(state.prompt)

    def test_the_row_goes_once_every_task_has_a_lane(self):
        state = editor()
        self.assertFalse(any("place task" in line for line in lines(state)))

    def test_d_on_a_lane_clears_that_schedule_only(self):
        state = editor("valid/modes.json")
        to_cell(state, LANE, "mode_manager")
        self.assertIn("d clear mode_manager", keys(state))
        state.key("d")
        self.assertIn("cleared mode_manager from operational", state.message)
        # The task and its window in the other schedule both stay.
        self.assertIn("mode_manager", state.schedule["tasks"])
        self.assertEqual(
            [w["task"] for w in state.cycles()[0][1]["windows"]], ["sense", "control"]
        )
        self.assertEqual([w["task"] for w in state.cycles()[1][1]["windows"]], ["mode_manager"])

    def test_clearing_a_lane_of_several_windows_asks_first(self):
        state = editor("valid/repeated-task.json")
        to_cell(state, LANE, "sense")
        state.key("d")
        self.assertIn("2 windows here", state.message)
        self.assertEqual(len(state.cycles()[0][1]["windows"]), 2)
        state.key("D")
        self.assertEqual(state.cycles()[0][1]["windows"], [])
        self.assertIn("sense", state.schedule["tasks"])

    def test_d_hides_a_lane_with_no_windows(self):
        state = editor("valid/modes.json")
        to_cell(state, PLACE_TASK)
        state.key("enter")
        typed(state, "sense")
        to_cell(state, LANE, "sense", schedule=1)
        self.assertIn("hide the empty lane", keys(state))
        state.key("d")
        self.assertIn("hid the empty sense lane", state.message)
        self.assertEqual(state.lanes(*state.cycles()[1]), ["mode_manager"])

    def test_a_renamed_task_keeps_its_revealed_lane(self):
        state = editor("valid/modes.json")
        to_cell(state, PLACE_TASK)
        state.key("enter")
        typed(state, "sense")
        to_cell(state, TASK_NAME, "sense")
        state.key("enter")
        typed(state, "sampler")
        self.assertEqual(state.lanes(*state.cycles()[1]), ["mode_manager", "sampler"])


class DeleteTests(unittest.TestCase):
    def test_d_on_a_window_deletes_it(self):
        state = editor()
        to_slot(state, "control", 3)
        state.key("d")
        self.assertEqual([w["task"] for w in state.cycles()[0][1]["windows"]], ["sense", "output"])

    def test_d_on_a_placed_task_asks_first(self):
        state = editor()
        state.key("d")
        self.assertIn("1 window", state.message)
        self.assertIn("control", state.schedule["tasks"])
        state.key("D")
        self.assertNotIn("control", state.schedule["tasks"])

    def test_the_last_task_is_kept(self):
        state = editor("valid/repeated-task.json")
        state.key("D")
        self.assertIn("at least one task", state.message)

    def test_d_on_a_schedule_asks_first_and_the_last_is_kept(self):
        state = editor("valid/modes.json")
        to_cell(state, SCHEDULE_NAME)
        state.key("d")
        self.assertIn("window", state.message)
        state.key("D")
        self.assertEqual(schedule_names(state.schedule), ["startup"])
        to_cell(state, SCHEDULE_NAME)
        state.key("D")
        self.assertIn("at least one schedule", state.message)


class FrameTests(unittest.TestCase):
    def test_a_task_shows_its_budget_before_the_bar_that_pictures_it(self):
        drawn = lines(editor())[3]
        self.assertEqual(drawn.split(), ["control", "3", "slots", "█" * 14])

    def test_the_cursor_cell_is_marked(self):
        state = editor()
        marked = [row for row in frame(state) if any(style == SELECTED for _, style in row)]
        self.assertEqual(len(marked), 1)
        self.assertEqual(
            "".join(text for text, style in marked[0] if style == SELECTED), "control"
        )

    def test_a_schedule_draws_a_lane_only_for_the_tasks_it_places(self):
        """A lane is a task's use of one cycle, so an unused task has none."""
        drawn = lines(editor("valid/modes.json"))
        startup = drawn[next(i for i, line in enumerate(drawn) if "schedule startup" in line) :]
        block = startup[: next(i for i, line in enumerate(startup) if "place task" in line)]
        self.assertTrue(any(line.startswith("  mode_manager") for line in block))
        for task in ("control", "sense"):
            self.assertFalse(any(line.startswith(f"  {task}") for line in block), task)

    def test_both_rows_that_add_something_are_shown(self):
        drawn = lines(editor())
        self.assertTrue(any("+ new task" in line for line in drawn))
        self.assertTrue(any("+ new schedule" in line for line in drawn))

    def test_the_keys_match_the_cell_under_the_cursor(self):
        state = editor()
        self.assertIn("enter rename", keys(state))
        to_cell(state, NEW_TASK)
        self.assertIn("enter add a task", keys(state))
        to_cell(state, SCHEDULE_SLOTS)
        self.assertIn("cycle length", keys(state))
        to_slot(state, "sense", 7)
        self.assertIn("place sense here", keys(state))
        to_slot(state, "sense", 0)
        self.assertIn("pick up", keys(state))

    def test_the_status_reports_issues_as_they_appear(self):
        state = editor()
        self.assertIn("valid", status(state))
        to_slot(state, "sense", 0)
        # sense then covers slots 1 and 2, where control already starts. While
        # it is held the status names that; once put down, the checks do.
        press(state, "enter", "right")
        self.assertIn("sense would overlap control", status(state))
        state.key("enter")
        self.assertIn("overlaps", status(state))

    def test_the_frame_fills_the_height(self):
        for height in (8, 20, 40):
            with self.subTest(height=height):
                state = editor(height=height)
                self.assertEqual(len(lines(state)), height)

    def test_the_view_scrolls_down_to_keep_the_cursor_visible(self):
        state = editor("valid/modes.json", height=10)
        self.assertEqual(state.first_row, 0)
        to_cell(state, NEW_SCHEDULE)
        state.scroll()
        self.assertGreater(state.first_row, 0)
        self.assertTrue(any(any(style == SELECTED for _, style in row) for row in frame(state)))


class WritingTests(unittest.TestCase):
    def test_writing_formats_the_file_and_clears_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schedule.json"
            path.write_text((FIXTURES / "valid/control-loop.json").read_text())
            state = Editor(str(path), load_schedule(path), width=76, height=30)
            to_slot(state, "sense", 7)
            state.key("enter")
            self.assertTrue(state.modified)
            state.key("w")
            self.assertIn("written to", state.message)
            self.assertFalse(state.modified)
            self.assertIn('{ "task": "sense", "start_slot": 7 }', path.read_text())

    def test_writing_is_refused_while_the_schedule_has_issues(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schedule.json"
            original = (FIXTURES / "invalid/overlap.json").read_text()
            path.write_text(original)
            state = Editor(str(path), load_schedule(path), width=76, height=30)
            state.key("w")
            self.assertIn("not written", state.message)
            self.assertEqual(path.read_text(), original)

    def test_quitting_with_unsaved_changes_needs_confirming(self):
        state = editor()
        state.key("q")
        self.assertTrue(state.finished)

        state = editor()
        to_slot(state, "sense", 7)
        state.key("enter")
        state.key("q")
        self.assertFalse(state.finished)
        self.assertIn("unsaved changes", state.message)
        state.key("Q")
        self.assertTrue(state.finished)


class InterruptTests(unittest.TestCase):
    def test_an_interrupt_asks_before_losing_work_and_then_quits(self):
        state = editor()
        to_slot(state, "sense", 7)
        state.key("enter")
        state.key("interrupt")
        self.assertFalse(state.finished)
        self.assertIn("unsaved changes", state.message)
        state.key("interrupt")
        self.assertTrue(state.finished)

    def test_an_interrupt_quits_at_once_with_nothing_to_lose(self):
        state = editor()
        state.key("interrupt")
        self.assertTrue(state.finished)

    def test_another_key_withdraws_the_interrupt(self):
        state = editor()
        to_slot(state, "sense", 7)
        press(state, "enter", "interrupt", "down", "interrupt")
        self.assertFalse(state.finished)


class EndOfInputTests(unittest.TestCase):
    def test_the_editor_stops_when_the_terminal_goes(self):
        """A lost terminal leaves nobody to ask, so the loop must not spin."""
        state = editor()
        to_slot(state, "sense", 7)
        state.key("enter")
        state.key("eof")
        self.assertTrue(state.finished)


class TerminalTests(unittest.TestCase):
    """The curses layer, which only names keys and paints what frame() returns."""

    def test_keys_are_named(self):
        self.assertEqual(key_name(curses.KEY_LEFT), "left")
        self.assertEqual(key_name(curses.KEY_DOWN), "down")
        self.assertEqual(key_name(10), "enter")
        self.assertEqual(key_name(27), "escape")
        self.assertEqual(key_name(ord("w")), "w")
        self.assertEqual(key_name(-1), "eof")

    def test_painting_places_spans_and_keeps_the_last_column_clear(self):
        class Screen:
            def __init__(self):
                self.written = []

            def erase(self):
                self.written.append("erase")

            def refresh(self):
                self.written.append("refresh")

            def addstr(self, row, column, text, attribute):
                self.written.append((row, column, text))

            def move(self, row, column):
                self.written.append(("move", row, column))

        screen = Screen()
        paint(screen, [[("ab", ""), ("cd", "dim")], [("x" * 20, "")]], 10, lambda style: 0)
        self.assertEqual(
            screen.written,
            ["erase", (0, 0, "ab"), (0, 2, "cd"), (1, 0, "x" * 9), "refresh"],
        )

    def test_painting_rests_the_cursor_where_it_is_asked_to(self):
        class Screen:
            def __init__(self):
                self.moved = None

            def erase(self):
                pass

            def refresh(self):
                pass

            def addstr(self, row, column, text, attribute):
                pass

            def move(self, row, column):
                self.moved = (row, column)

        screen = Screen()
        paint(screen, [[("ab", "")]], 10, lambda style: 0, spot=(4, 7))
        self.assertEqual(screen.moved, (4, 7))


if __name__ == "__main__":
    unittest.main()
