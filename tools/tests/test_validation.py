import json
import unittest
from importlib.resources import files
from pathlib import Path

from jsonschema import Draft202012Validator

from zule_tools.validation import validate_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"


def read_fixture(name):
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


class ValidationTests(unittest.TestCase):
    def test_schema_and_example(self):
        schema = json.loads(files("zule_tools").joinpath("schedule.schema.json").read_text())
        Draft202012Validator.check_schema(schema)
        self.assertEqual(validate_schedule(read_fixture("valid/control-loop.json")), [])

    def test_structural_errors_do_not_crash_semantic_checks(self):
        for name in (
            "generic/null.json",
            "generic/empty-array.json",
            "generic/empty-object.json",
            "invalid/invalid-tasks-type.json",
        ):
            with self.subTest(fixture=name):
                self.assertTrue(validate_schedule(read_fixture(name)))

    def test_unknown_task(self):
        issues = validate_schedule(read_fixture("invalid/unknown-task.json"))
        self.assertTrue(any(i.path == "/cycle/windows/0/task" for i in issues))

    def test_order_overlap_and_cycle_bounds(self):
        cases = (
            ("invalid/overlap.json", "overlaps"),
            ("invalid/unordered-windows.json", "increasing"),
            ("invalid/equal-starts.json", "increasing"),
            ("invalid/end-beyond-cycle.json", "beyond cycle end"),
            ("invalid/start-outside-cycle.json", "outside"),
        )
        for name, expected in cases:
            with self.subTest(fixture=name):
                issues = validate_schedule(read_fixture(name))
                self.assertTrue(any(expected in i.message for i in issues))

    def test_nested_overlap(self):
        issues = validate_schedule(read_fixture("invalid/nested-overlap.json"))
        self.assertTrue(any(i.path == "/cycle/windows/2" and "overlaps" in i.message for i in issues))

    def test_repeated_tasks_touching_windows_and_exact_cycle_end(self):
        self.assertEqual(validate_schedule(read_fixture("valid/repeated-task.json")), [])


if __name__ == "__main__":
    unittest.main()
