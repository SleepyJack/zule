import unittest
from pathlib import Path

from zule_schedule.rendering import render_schedule
from zule_schedule.schedule_io import load_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"


def lanes(text):
    """Return each labelled line of a rendered block, keyed by its label."""
    result = {}
    for line in text.splitlines():
        label = line.split("  ")[0].strip()
        if label and not line.startswith(" ") and not label.startswith("schedule "):
            result[label] = line
    return result


class RenderingTests(unittest.TestCase):
    def render(self, name, **kwargs):
        return render_schedule(load_schedule(FIXTURES / name), **kwargs)

    def test_lanes_are_in_task_id_order(self):
        """Lanes follow the generated task IDs, which are in task-name order."""
        text = self.render("valid/control-loop.json", width=76)
        labels = [label for label in lanes(text) if label != "idle"]
        self.assertEqual(labels, ["control", "output", "sense"])

    def test_windows_start_at_their_slot(self):
        text = self.render("valid/control-loop.json", width=76)
        # A label column of 9, then 5 columns a slot: sense at slot 0, control
        # at slot 2, and output at slot 5, each a column short of its end so
        # that touching windows stay apart.
        self.assertEqual(lanes(text)["sense"].index("█"), 9)
        self.assertEqual(lanes(text)["sense"].count("█"), 9)
        self.assertEqual(lanes(text)["control"].index("█"), 19)
        self.assertEqual(lanes(text)["control"].count("█"), 14)
        self.assertEqual(lanes(text)["output"].index("█"), 34)

    def test_idle_slots_and_usage(self):
        text = self.render("valid/control-loop.json", width=76)
        self.assertEqual(lanes(text)["idle"].count("░"), 3 * 4)
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
        text = self.render("valid/modes.json", width=90)
        self.assertLess(text.index("schedule operational"), text.index("schedule startup"))
        self.assertEqual(text.count("schedule "), 2)

    def test_task_without_a_window_is_noted(self):
        startup = self.render("valid/modes.json", width=90).split("schedule startup")[1]
        self.assertIn("2 slots (no window)", lanes(startup)["sense"])
        self.assertNotIn("█", lanes(startup)["sense"])
        self.assertIn("1 slot", lanes(startup)["mode_manager"])

    def test_unused_task_still_has_a_lane(self):
        text = self.render("valid/unused-task.json", width=76)
        self.assertIn("(no window)", text)

    def test_overlapping_windows_of_one_task_are_marked(self):
        """An overlap renders rather than being hidden, since seeing it is the point."""
        text = self.render("invalid/overlap.json", width=60)
        self.assertIn("▒", lanes(text)["sense"])

    def test_window_beyond_the_cycle_is_clipped_to_the_grid(self):
        text = self.render("invalid/end-beyond-cycle.json", width=76)
        for line in text.splitlines():
            self.assertLessEqual(len(line), 76)

    def test_unknown_task_is_left_to_validation(self):
        text = self.render("invalid/unknown-task.json", width=76)
        self.assertNotIn("missing", text)

    def test_long_cycles_scale_to_fit(self):
        schedule = {
            "slot_us": 1000,
            "tasks": {"a": {"budget_slots": 3}},
            "cycle": {"slots": 400, "windows": [{"task": "a", "start_slot": 100}]},
        }
        text = render_schedule(schedule, width=76)
        self.assertIn("scaled to fit", text)
        self.assertIn("█", lanes(text)["a"])
        for line in text.splitlines():
            self.assertLessEqual(len(line), 76)

    def test_lines_fit_the_requested_width(self):
        for name in ("valid/control-loop.json", "valid/modes.json", "valid/repeated-task.json"):
            for width in (40, 60, 80, 120):
                with self.subTest(fixture=name, width=width):
                    for line in self.render(name, width=width).splitlines():
                        self.assertLessEqual(len(line), width)

    def test_colour_is_optional(self):
        self.assertNotIn("\033", self.render("valid/control-loop.json", width=76))
        coloured = self.render("valid/control-loop.json", width=76, color=True)
        self.assertIn("\033", coloured)
        # Colour is decoration only, so removing it gives the plain rendering.
        stripped = "".join(
            part.split("m", 1)[1] if index else part
            for index, part in enumerate(coloured.split("\033["))
        )
        self.assertEqual(stripped, self.render("valid/control-loop.json", width=76))


if __name__ == "__main__":
    unittest.main()
