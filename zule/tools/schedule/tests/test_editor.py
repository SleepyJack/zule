import tempfile
import unittest
from pathlib import Path

from zule_schedule.editing import delete_window, move_window
import curses

from zule_schedule.editor import SELECTED, Editor, frame, key_name, paint
from zule_schedule.schedule_io import load_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"
WIDTH = 76
HEIGHT = 20


def editor(name="valid/control-loop.json", path="schedule.json"):
    return Editor(path, load_schedule(FIXTURES / name))


def rows(editor_):
    editor_.ensure_visible(WIDTH)
    return frame(editor_, WIDTH, HEIGHT)


def text(row):
    return "".join(part for part, _ in row)


class EditingTests(unittest.TestCase):
    """The edits themselves, which never change the schedule given to them."""

    def setUp(self):
        self.schedule = load_schedule(FIXTURES / "valid/control-loop.json")
        self.before = load_schedule(FIXTURES / "valid/control-loop.json")

    def test_moving_leaves_the_original_schedule_alone(self):
        moved, _ = move_window(self.schedule, "main", 0, 1)
        self.assertEqual(self.schedule, self.before)
        self.assertEqual(moved["cycle"]["windows"][0]["start_slot"], 1)

    def test_a_window_moved_past_another_is_reordered(self):
        """The format requires increasing start slots, so the list is kept sorted."""
        # sense is at slot 0 with control at 2; moving sense to 3 puts it second.
        moved, index = move_window(self.schedule, "main", 0, 3)
        self.assertEqual(index, 1)
        self.assertEqual(moved["cycle"]["windows"][index]["task"], "sense")
        starts = [window["start_slot"] for window in moved["cycle"]["windows"]]
        self.assertEqual(starts, sorted(starts))

    def test_a_window_cannot_start_outside_its_cycle(self):
        unchanged, index = move_window(self.schedule, "main", 0, -1)
        self.assertIs(unchanged, self.schedule)
        self.assertEqual(index, 0)
        self.assertIs(move_window(self.schedule, "main", 2, 5)[0], self.schedule)

    def test_deleting_leaves_the_original_schedule_alone(self):
        deleted, index = delete_window(self.schedule, "main", 2)
        self.assertEqual(self.schedule, self.before)
        self.assertEqual(len(deleted["cycle"]["windows"]), 2)
        self.assertEqual(index, 1)


class SelectionTests(unittest.TestCase):
    def test_selection_moves_and_wraps(self):
        state = editor()
        self.assertEqual(state.selected()["task"], "sense")
        state.key("down")
        self.assertEqual(state.selected()["task"], "control")
        state.key("up")
        state.key("up")
        self.assertEqual(state.selected()["task"], "output")

    def test_moving_a_window_keeps_it_selected(self):
        state = editor()
        state.key("right")
        state.key("right")
        state.key("right")
        self.assertEqual(state.selected(), {"task": "sense", "start_slot": 3})
        self.assertTrue(state.modified)

    def test_a_refused_move_says_why(self):
        state = editor()
        state.key("left")
        self.assertIn("outside its cycle", state.message)
        self.assertFalse(state.modified)

    def test_deleting_the_selected_window(self):
        state = editor()
        state.key("d")
        self.assertEqual([window["task"] for window in state.windows()], ["control", "output"])
        self.assertEqual(state.selected()["task"], "control")

    def test_tab_moves_to_the_next_schedule(self):
        state = editor("valid/modes.json")
        self.assertEqual(state.current()[0], "operational")
        state.key("tab")
        self.assertEqual(state.current()[0], "startup")
        state.key("tab")
        self.assertEqual(state.current()[0], "operational")


class WritingTests(unittest.TestCase):
    def test_writing_formats_the_file_and_clears_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schedule.json"
            path.write_text((FIXTURES / "valid/control-loop.json").read_text())
            state = Editor(str(path), load_schedule(path))
            # Move the last window, which has room after it, so that the edit
            # stays valid and can be written.
            state.key("up")
            state.key("right")
            self.assertTrue(state.modified)
            state.key("w")
            self.assertIn("written to", state.message)
            self.assertFalse(state.modified)
            self.assertIn('{ "task": "output", "start_slot": 6 }', path.read_text())

    def test_writing_is_refused_while_the_schedule_has_issues(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schedule.json"
            original = (FIXTURES / "invalid/overlap.json").read_text()
            path.write_text(original)
            state = Editor(str(path), load_schedule(path))
            state.key("w")
            self.assertIn("not written", state.message)
            self.assertEqual(path.read_text(), original)

    def test_quitting_with_unsaved_changes_needs_confirming(self):
        state = editor()
        state.key("q")
        self.assertTrue(state.finished)

        state = editor()
        state.key("right")
        state.key("q")
        self.assertFalse(state.finished)
        self.assertIn("unsaved changes", state.message)
        state.key("Q")
        self.assertTrue(state.finished)


class FrameTests(unittest.TestCase):
    def test_the_frame_fills_the_height(self):
        for height in (8, 20, 40):
            with self.subTest(height=height):
                state = editor()
                state.ensure_visible(WIDTH)
                self.assertEqual(len(frame(state, WIDTH, height)), height)

    def test_the_tasks_come_before_the_schedule(self):
        lines = [text(row) for row in rows(editor())]
        self.assertLess(lines.index("tasks"), next(
            index for index, line in enumerate(lines) if line.startswith("schedule main")
        ))

    def test_only_the_selected_window_is_highlighted(self):
        state = editor()
        highlighted = [row for row in rows(state) if any(style == SELECTED for _, style in row)]
        self.assertEqual(len(highlighted), 1)
        self.assertIn("sense", text(highlighted[0]))
        selected = "".join(part for part, style in highlighted[0] if style == SELECTED)
        self.assertEqual(selected, "█" * 9)

    def test_the_title_shows_the_path_and_whether_it_is_modified(self):
        state = editor(path="config/schedule.json")
        self.assertEqual(text(rows(state)[0]), "config/schedule.json")
        state.key("right")
        self.assertIn("modified", text(rows(state)[0]))

    def test_the_status_reports_issues(self):
        state = editor("invalid/overlap.json")
        self.assertIn("overlaps", text(rows(state)[-2]))
        self.assertIn("valid", text(rows(editor())[-2]))

    def test_a_long_cycle_scrolls_to_the_selection(self):
        schedule = {
            "slot_us": 1000,
            "tasks": {"sense": {"budget_slots": 2}},
            "cycle": {
                "slots": 40,
                "windows": [{"task": "sense", "start_slot": 0}, {"task": "sense", "start_slot": 30}],
            },
        }
        state = Editor("schedule.json", schedule)
        state.ensure_visible(WIDTH)
        self.assertEqual(state.first_slot, 0)
        state.key("down")
        state.ensure_visible(WIDTH)
        # The view holds 13 slots, and scrolls the least that shows slots 30-31.
        self.assertEqual(state.first_slot, 19)
        heading = next(text(row) for row in frame(state, WIDTH, HEIGHT) if "showing" in text(row))
        self.assertIn("showing 19-31", heading)


class TerminalTests(unittest.TestCase):
    """The curses layer, which only names keys and paints what frame() returns."""

    def test_keys_are_named(self):
        self.assertEqual(key_name(curses.KEY_LEFT), "left")
        self.assertEqual(key_name(curses.KEY_DOWN), "down")
        self.assertEqual(key_name(ord("\t")), "tab")
        self.assertEqual(key_name(ord("w")), "w")
        self.assertEqual(key_name(-1), "")

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

        screen = Screen()
        paint(screen, [[("ab", ""), ("cd", "dim")], [("x" * 20, "")]], 10, lambda style: 0)
        self.assertEqual(
            screen.written,
            ["erase", (0, 0, "ab"), (0, 2, "cd"), (1, 0, "x" * 9), "refresh"],
        )


if __name__ == "__main__":
    unittest.main()
