import tempfile
import unittest
from pathlib import Path

from zule_schedule.formatting import format_schedule
from zule_schedule.schedule_io import load_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"


class FormattingTests(unittest.TestCase):
    def test_short_values_stay_on_one_line(self):
        schedule = {
            "slot_us": 100,
            "tasks": {"sense": {"budget_slots": 2}},
            "cycle": {"slots": 4, "windows": [{"task": "sense", "start_slot": 0}]},
        }
        self.assertEqual(
            format_schedule(schedule),
            '{\n'
            '  "slot_us": 100,\n'
            '  "tasks": { "sense": { "budget_slots": 2 } },\n'
            '  "cycle": { "slots": 4, "windows": [{ "task": "sense", "start_slot": 0 }] }\n'
            '}\n',
        )

    def test_long_values_are_expanded(self):
        task = {"description": "Sample every input into one stable snapshot.", "budget_slots": 2}
        text = format_schedule({"tasks": {"a_fairly_long_task_name": task}})
        self.assertIn('    "a_fairly_long_task_name": {\n', text)
        self.assertIn('      "budget_slots": 2\n', text)
        for line in text.splitlines():
            self.assertLessEqual(len(line), 80)

    def test_a_value_too_long_to_fit_still_gets_a_line(self):
        text = format_schedule({"description": "x" * 200})
        self.assertEqual(len(text.splitlines()), 3)

    def test_keys_keep_their_order(self):
        text = format_schedule({"cycle": {"windows": [], "slots": 2}, "slot_us": 1})
        self.assertLess(text.index("windows"), text.index("slots"))
        self.assertLess(text.index("cycle"), text.index("slot_us"))

    def test_empty_containers_and_scalars(self):
        self.assertEqual(format_schedule({}), "{}\n")
        self.assertEqual(format_schedule([]), "[]\n")
        self.assertEqual(format_schedule(None), "null\n")

    def test_text_is_kept_as_it_is(self):
        text = format_schedule({"description": "ångström · 50 µs"})
        self.assertIn("ångström · 50 µs", text)

    def test_formatting_is_settled(self):
        """Formatting an already formatted schedule changes nothing further."""
        for name in ("valid/modes.json", "valid/control-loop.json"):
            with self.subTest(fixture=name):
                once = format_schedule(load_schedule(FIXTURES / name))
                with tempfile.TemporaryDirectory() as directory:
                    path = Path(directory) / "schedule.json"
                    path.write_text(once)
                    self.assertEqual(format_schedule(load_schedule(path)), once)


if __name__ == "__main__":
    unittest.main()
