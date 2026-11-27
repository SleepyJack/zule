import io
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from zule_schedule.cli import main


FIXTURES = Path(__file__).resolve().parent / "fixtures"


class CLITests(unittest.TestCase):
    def test_generate_success(self):
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory) / "generated"
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(["generate", str(FIXTURES / "valid/control-loop.json"), str(output_dir)]), 0)
            self.assertTrue((output_dir / "zule_schedule_definition.c").is_file())
            self.assertTrue((output_dir / "zule_task_ids.h").is_file())
            self.assertTrue((output_dir / "zule_schedule_ids.h").is_file())
            self.assertIn(": generated", output.getvalue())

    def test_generate_rejects_invalid_input(self):
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory) / "generated"
            with redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(main(["generate", str(FIXTURES / "invalid/overlap.json"), str(output_dir)]), 1)
            self.assertIn("overlaps", errors.getvalue())
            self.assertFalse(output_dir.exists())

    def test_generate_reports_output_error(self):
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory) / "file"
            output_dir.write_text("not a directory")
            with redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(main(["generate", str(FIXTURES / "valid/control-loop.json"), str(output_dir)]), 1)
            self.assertIn(str(output_dir), errors.getvalue())

    def test_format_writes_only_files_that_differ(self):
        with tempfile.TemporaryDirectory() as directory:
            canonical = Path(directory) / "canonical.json"
            canonical.write_text((FIXTURES / "valid/control-loop.json").read_text())
            untidy = Path(directory) / "untidy.json"
            untidy.write_text('{"slot_us":100,"tasks":{},"cycle":{"slots":1,"windows":[]}}')
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(["format", str(canonical), str(untidy)]), 0)
            self.assertNotIn("canonical.json", output.getvalue())
            self.assertIn("untidy.json: formatted", output.getvalue())
            self.assertTrue(untidy.read_text().startswith("{\n  \"slot_us\": 100,"))

    def test_format_check_reports_without_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            untidy = Path(directory) / "untidy.json"
            original = '{"slot_us":100,"tasks":{},"cycle":{"slots":1,"windows":[]}}'
            untidy.write_text(original)
            with redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(main(["format", "--check", str(untidy)]), 1)
            self.assertIn("needs formatting", errors.getvalue())
            self.assertEqual(untidy.read_text(), original)

    def test_format_reports_unreadable_input(self):
        with tempfile.TemporaryDirectory() as directory:
            with redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(main(["format", str(Path(directory) / "missing.json")]), 1)
            self.assertIn("missing.json", errors.getvalue())

    def test_show_renders_every_schedule(self):
        with redirect_stdout(io.StringIO()) as output:
            self.assertEqual(main(["show", str(FIXTURES / "valid/modes.json"), "--width", "90"]), 0)
        self.assertIn("schedule operational", output.getvalue())
        self.assertIn("schedule startup", output.getvalue())

    def test_show_renders_a_schedule_with_semantic_issues(self):
        with redirect_stdout(io.StringIO()) as output, redirect_stderr(io.StringIO()) as errors:
            self.assertEqual(main(["show", str(FIXTURES / "invalid/overlap.json")]), 1)
        self.assertIn("schedule main", output.getvalue())
        self.assertIn("overlaps", errors.getvalue())

    def test_show_reports_structural_issues_without_rendering(self):
        with redirect_stdout(io.StringIO()) as output, redirect_stderr(io.StringIO()) as errors:
            self.assertEqual(main(["show", str(FIXTURES / "generic/empty-object.json")]), 1)
        self.assertEqual(output.getvalue(), "")
        self.assertIn("required property", errors.getvalue())

    def test_show_is_not_coloured_when_redirected(self):
        with redirect_stdout(io.StringIO()) as output:
            self.assertEqual(main(["show", str(FIXTURES / "valid/control-loop.json")]), 0)
        self.assertNotIn("\033", output.getvalue())

    def test_validate_success(self):
        with redirect_stdout(io.StringIO()) as output:
            self.assertEqual(main(["validate", str(FIXTURES / "valid/control-loop.json")]), 0)
        self.assertIn(": valid", output.getvalue())

    def test_validate_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            path = FIXTURES / "generic/empty-object.json"
            with redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(main(["validate", str(path)]), 1)
            self.assertIn("required property", errors.getvalue())
            with redirect_stderr(io.StringIO()):
                self.assertEqual(main(["validate", str(Path(directory) / "missing.json")]), 1)


if __name__ == "__main__":
    unittest.main()
