import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from zule_tools.generation import generate_schedule
from zule_tools.schedule_io import load_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"


class GenerationTests(unittest.TestCase):
    def test_generated_control_loop_compiles_and_preserves_schedule(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler is required for generated-code integration test")
        schedule = load_schedule(FIXTURES / "valid/control-loop.json")
        with tempfile.TemporaryDirectory() as directory:
            output = generate_schedule(schedule, directory)
            harness = Path(directory) / "test.c"
            harness.write_text('''#include <assert.h>
#include "schedule.c"
static unsigned called;
void zule_task_sense(void) { called = 1; }
void zule_task_control(void) { called = 2; }
void zule_task_output(void) { called = 3; }
int main(void) {
    assert(zule_slot_us == 100U);
    assert(zule_cycle_slots == 10U);
    assert(zule_window_count == 3U);
    const uint32_t starts[] = {0U, 2U, 5U};
    const uint32_t budgets[] = {2U, 3U, 2U};
    for (size_t i = 0; i < zule_window_count; ++i) {
        assert(zule_windows[i].start_slot == starts[i]);
        assert(zule_windows[i].budget_slots == budgets[i]);
        zule_windows[i].task();
        assert(called == i + 1U);
    }
    return 0;
}
''')
            executable = Path(directory) / "test"
            include_dir = Path(__file__).resolve().parents[2] / "core/inc"
            subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(include_dir), str(harness), "-o", str(executable)], check=True, capture_output=True, text=True)
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)
            original = output.read_bytes()
            self.assertEqual(generate_schedule(schedule, directory).read_bytes(), original)

    def test_repeated_task_is_declared_once(self):
        schedule = load_schedule(FIXTURES / "valid/repeated-task.json")
        with tempfile.TemporaryDirectory() as directory:
            source = generate_schedule(schedule, directory).read_text()
            self.assertEqual(source.count("extern void zule_task_sense(void);"), 1)
            self.assertIn("{ zule_task_sense, 0U, 2U }", source)
            self.assertIn("{ zule_task_sense, 2U, 2U }", source)

    def test_invalid_input_preserves_existing_output(self):
        for name, message in (("invalid/overlap.json", "overlaps"), ("valid/large-slot.json", "uint32_t")):
            with self.subTest(fixture=name), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "schedule.c"
                output.write_text("existing output")
                with self.assertRaisesRegex(ValueError, message):
                    generate_schedule(load_schedule(FIXTURES / name), directory)
                self.assertEqual(output.read_text(), "existing output")


if __name__ == "__main__":
    unittest.main()
