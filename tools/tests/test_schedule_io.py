import tempfile
import unittest
from pathlib import Path

from zule_tools.schedule_io import load_schedule


class ScheduleIOTests(unittest.TestCase):
    def test_loads_json_without_schedule_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schedule.json"
            path.write_text('{"tasks": {}}')
            self.assertEqual(load_schedule(path), {"tasks": {}})

    def test_rejects_invalid_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "schedule.json"
            for text in ('{"tasks": {}, "tasks": {}}', '{"value": NaN}', '{"value": Infinity}', '{'):
                with self.subTest(text=text):
                    path.write_text(text)
                    with self.assertRaises(ValueError):
                        load_schedule(path)


if __name__ == "__main__":
    unittest.main()
