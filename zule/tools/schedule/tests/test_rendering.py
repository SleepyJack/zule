import unittest
from pathlib import Path

from zule_schedule.rendering import render_schedule
from zule_schedule.schedule_io import load_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"

# The label column of the control-loop fixture: a margin of 2, the longest of
# "control" and "idle", then a gap of 2. Slots are five columns, of which a
# window's last is blank.
CONTROL_LOOP_LABEL = 11
SLOT = 5


def block(text, heading):
    """Return the block a heading introduces, up to the next heading.

    Headings start at column 0 and everything in a block is indented, so the
    blank lines between staves do not end it.
    """
    lines = text.splitlines()
    start = next(index for index, line in enumerate(lines) if line.startswith(heading))
    end = next(
        (
            index
            for index in range(start + 1, len(lines))
            if lines[index] and not lines[index].startswith(" ")
        ),
        len(lines),
    )
    return "\n".join(lines[start:end])


def lanes(text):
    """Return each indented lane of a block, keyed by its label.

    The ruler and the usage footer are indented too, but start with a number.
    """
    result = {}
    for line in text.splitlines():
        label = line.split()[0] if line.startswith("  ") and line.strip() else ""
        if label and (label[0].isalpha() or label[0] == "_"):
            result[label] = line
    return result


class TaskBlockTests(unittest.TestCase):
    def test_lists_every_task_in_task_id_order(self):
        """The order is task-name order, which is the order of the task IDs."""
        text = render_schedule(load_schedule(FIXTURES / "valid/modes.json"), width=76)
        tasks = lanes(block(text, "tasks"))
        self.assertEqual(list(tasks), ["control", "mode_manager", "sense"])
        self.assertIn("3 slots", tasks["control"])
        self.assertIn("1 slot", tasks["mode_manager"])

    def test_lists_a_task_no_schedule_places(self):
        text = render_schedule(load_schedule(FIXTURES / "valid/unused-task.json"), width=76)
        self.assertEqual(len(lanes(block(text, "tasks"))), 2)

    def test_the_budget_comes_before_the_bar_that_pictures_it(self):
        """The editor puts the number first, where it is edited, so show does too."""
        text = render_schedule(load_schedule(FIXTURES / "valid/modes.json"), width=76)
        self.assertEqual(
            lanes(block(text, "tasks"))["control"].split(),
            ["control", "3", "slots", "█" * 14],
        )

    def test_budget_is_drawn_as_a_window_of_that_task_is(self):
        """The shared slot width is the point of the task block: sizes compare."""
        text = render_schedule(load_schedule(FIXTURES / "valid/control-loop.json"), width=76)
        for task in ("control", "output", "sense"):
            with self.subTest(task=task):
                budget = lanes(block(text, "tasks"))[task].count("█")
                window = lanes(block(text, "schedule main"))[task].count("█")
                self.assertEqual(budget, window)


class CycleBlockTests(unittest.TestCase):
    def render(self, name, **kwargs):
        return render_schedule(load_schedule(FIXTURES / name), **kwargs)

    def test_lanes_cover_only_the_tasks_a_schedule_places(self):
        startup = block(self.render("valid/modes.json", width=76), "schedule startup")
        self.assertEqual([label for label in lanes(startup)], ["mode_manager", "idle"])

    def test_windows_start_at_their_slot(self):
        text = block(self.render("valid/control-loop.json", width=76), "schedule main")
        self.assertEqual(lanes(text)["sense"].index("█"), CONTROL_LOOP_LABEL)
        self.assertEqual(lanes(text)["sense"].count("█"), 2 * SLOT - 1)
        self.assertEqual(lanes(text)["control"].index("█"), CONTROL_LOOP_LABEL + 2 * SLOT)
        self.assertEqual(lanes(text)["control"].count("█"), 3 * SLOT - 1)
        self.assertEqual(lanes(text)["output"].index("█"), CONTROL_LOOP_LABEL + 5 * SLOT)

    def test_idle_slots_and_usage(self):
        text = block(self.render("valid/control-loop.json", width=76), "schedule main")
        self.assertEqual(lanes(text)["idle"].count("░"), 3 * SLOT - 1)
        self.assertIn("7/10 slots reserved · 70%", text)

    def test_header_reports_cycle_and_slot_durations(self):
        self.assertIn(
            "schedule main · 10 slots · 1 ms · slot 100 us",
            self.render("valid/control-loop.json", width=76),
        )
        seconds = {
            "slot_us": 2000000,
            "tasks": {"a": {"budget_slots": 1}},
            "cycle": {"slots": 1, "windows": [{"task": "a", "start_slot": 0}]},
        }
        self.assertIn("1 slot · 2 s · slot 2 s", render_schedule(seconds, width=76))

    def test_every_schedule_renders_in_id_order(self):
        text = self.render("valid/modes.json", width=76)
        self.assertLess(text.index("schedule operational"), text.index("schedule startup"))
        self.assertEqual(text.count("schedule "), 2)

    def test_overlapping_windows_of_one_task_are_marked(self):
        """An overlap renders rather than being hidden, since seeing it is the point."""
        text = self.render("invalid/overlap.json", width=76)
        self.assertIn("▒", lanes(block(text, "schedule main"))["sense"])

    def test_window_beyond_the_cycle_is_clipped_to_the_cycle(self):
        text = self.render("invalid/end-beyond-cycle.json", width=76)
        for line in text.splitlines():
            self.assertLessEqual(len(line), 76)

    def test_unknown_task_is_left_to_validation(self):
        self.assertNotIn("missing", self.render("invalid/unknown-task.json", width=76))


class StaveTests(unittest.TestCase):
    schedule = {
        "slot_us": 1000,
        "tasks": {"sense": {"budget_slots": 3}},
        "cycle": {"slots": 28, "windows": [{"task": "sense", "start_slot": 20}]},
    }

    def test_a_long_cycle_continues_on_further_staves(self):
        text = render_schedule(self.schedule, width=60)
        rulers = [line for line in text.splitlines() if line.lstrip().startswith("0 ")]
        self.assertEqual(len(rulers), 1)
        # Each stave is labelled with its own slots, so the window at slot 20
        # appears under 20 on the third stave.
        self.assertIn("  20   21   22", text)
        self.assertEqual(block(text, "schedule main").count("  sense"), 3)

    def test_staves_keep_the_slot_width(self):
        for width in (40, 60, 80, 120):
            with self.subTest(width=width):
                text = render_schedule(self.schedule, width=width)
                for line in text.splitlines():
                    self.assertLessEqual(len(line), width)
                # A window keeps its drawn width however the staves split it.
                drawn = sum(
                    line.count("█") for line in block(text, "schedule main").splitlines()
                )
                self.assertEqual(drawn, 3 * SLOT - 1)

    def test_a_narrow_terminal_still_shows_whole_slots(self):
        text = render_schedule(self.schedule, width=4)
        self.assertIn("█", text)


class ColourTests(unittest.TestCase):
    def test_colour_is_optional_and_decoration_only(self):
        plain = render_schedule(load_schedule(FIXTURES / "valid/modes.json"), width=76)
        coloured = render_schedule(
            load_schedule(FIXTURES / "valid/modes.json"), width=76, color=True
        )
        self.assertNotIn("\033", plain)
        self.assertIn("\033", coloured)
        stripped = "".join(
            part.split("m", 1)[1] if index else part
            for index, part in enumerate(coloured.split("\033["))
        )
        self.assertEqual(stripped, plain)

    def test_a_task_keeps_one_colour_in_every_block(self):
        text = render_schedule(load_schedule(FIXTURES / "valid/modes.json"), width=76, color=True)
        codes = {
            line.split("m", 1)[0] + "m"
            for line in text.splitlines()
            if "mode_manager" in line and line.startswith("\033[")
        }
        self.assertEqual(len(codes), 1)


if __name__ == "__main__":
    unittest.main()
