import io
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from zule_tools.cli import main


FIXTURES = Path(__file__).resolve().parent / "fixtures"


class CLITests(unittest.TestCase):
    def test_generate_success(self):
        with tempfile.TemporaryDirectory() as directory:
            output_dir = Path(directory) / "generated"
            with redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(["generate", str(FIXTURES / "valid/control-loop.json"), str(output_dir)]), 0)
            self.assertTrue((output_dir / "schedule.c").is_file())
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
