import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from zule_schedule.generation import generate_schedule
from zule_schedule.schedule_io import load_schedule


FIXTURES = Path(__file__).resolve().parent / "fixtures"
ZULE_DIR = Path(__file__).resolve().parents[3]


def zule_sources() -> list[str]:
    """Return Zule's sources, the core and the modules, from the single list in zule_sources.txt."""
    lines = (ZULE_DIR / "zule_sources.txt").read_text(encoding="utf-8").splitlines()
    return [str(ZULE_DIR / line.strip()) for line in lines if line.strip() and not line.startswith("#")]


def compile_command(compiler: str, harness: Path, output: Path, executable: Path) -> list[str]:
    """Return a command compiling a harness and generated schedule with the core and test port."""
    core_dir = ZULE_DIR / "core"
    return [
        compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-I", str(core_dir / "api"), "-I", str(core_dir / "inc"),
        "-I", str(core_dir / "tests/port"), "-I", str(ZULE_DIR / "ports"),
        "-I", str(ZULE_DIR / "modules/api"), "-I", str(ZULE_DIR / "modules/inc"),
        str(harness), str(output), *zule_sources(), str(core_dir / "tests/port/zule_port.c"),
        "-o", str(executable),
    ]


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
#include "zule_task_ids.h"
#include "zule_task_ids.h"
#include "zule_schedule_ids.h"
#include "zule_modules_task_data.h"
#include "zule_schedule_definition.h"
#include "zule_task_stats.h"
#include <string.h>
static unsigned called;
void zule_task_sense(void) { called = 1; }
void zule_task_control(void) { called = 2; }
void zule_task_output(void) { called = 3; }
int main(void) {
    /* A single cycle is the one schedule, main, with ID 0. */
    const zule_schedules_t *schedules = zule_schedules_get();
    assert(schedules == zule_schedules_get());
    assert(schedules->schedule_count == 1U);
    assert(ZULE_SCHEDULE_ID_MAIN == 0);
    assert(zule_schedules_validate(schedules) == ZULE_RESULT_OK);
    assert(schedules->slot_us == 100U);
    assert(schedules->task_count == 3U);
    const zule_schedule_t *schedule = &schedules->schedules[ZULE_SCHEDULE_ID_MAIN];
    assert(schedule->cycle_slots == 10U);
    assert(schedule->window_count == 3U);
    const uint32_t starts[] = {0U, 2U, 5U};
    const uint32_t budgets[] = {2U, 3U, 2U};
    assert(ZULE_TASK_ID_CONTROL == 0);
    assert(ZULE_TASK_ID_OUTPUT == 1);
    assert(ZULE_TASK_ID_SENSE == 2);
    const zule_task_id_t ids[] = {ZULE_TASK_ID_SENSE, ZULE_TASK_ID_CONTROL, ZULE_TASK_ID_OUTPUT};
#if ZULE_CONFIG_TASK_NAMES
    const char *const names[] = {"sense", "control", "output"};
#endif
    for (size_t i = 0; i < schedule->window_count; ++i) {
        assert(schedule->windows[i].start_slot == starts[i]);
        assert(schedule->windows[i].budget_slots == budgets[i]);
        assert(schedule->windows[i].task->id == ids[i]);
#if ZULE_CONFIG_TASK_NAMES
        assert(strcmp(zule_task_name_get(ids[i]), names[i]) == 0);
        assert(strcmp(schedule->windows[i].task->name, names[i]) == 0);
#else
        assert(zule_task_name_get(ids[i]) == NULL);
#endif
        schedule->windows[i].task->function();
        assert(called == i + 1U);
    }
    assert(zule_task_name_get(UINT32_MAX) == NULL);
#if ZULE_MODULES_TASK_DATA
    /* One zeroed entry per task, through the generated storage. */
    assert(zule_modules_task_data_get() == zule_modules_task_data_get());
    zule_task_stats_reset();
    for (uint32_t id = 0U; id < 3U; ++id) {
        zule_task_stats_t stats = {.executions = 1U};
        assert(zule_task_stats_get(id, &stats));
        assert(stats.executions == 0U);
    }
    zule_task_stats_t stats;
    assert(!zule_task_stats_get(3U, &stats));
#endif
    return 0;
}
''')
            executable = Path(directory) / "test"
            command = compile_command(compiler, harness, output, executable)
            (Path(directory) / "zule_user_config.h").write_text("#define ZULE_CONFIG_TASK_NAMES 1\n")
            empty_config = Path(directory) / "empty_config"
            empty_config.mkdir()
            (empty_config / "zule_user_config.h").write_text("/* Use Zule defaults. */\n")
            modes = (
                [],
                ["-DZULE_CONFIG_TASK_NAMES=1"],
                ["-DZULE_CONFIG_TASK_NAMES=0"],
                ["-DZULE_USE_USER_CONFIG=1", "-I", directory],
                ["-DZULE_USE_USER_CONFIG=1", "-I", str(empty_config)],
                ["-DZULE_CONFIG_TIMEBASE=1", "-DZULE_CONFIG_MODULE_TASK_STATS=1"],
            )
            for flags in modes:
                with self.subTest(flags=flags):
                    subprocess.run(command + flags, check=True, capture_output=True, text=True)
                    subprocess.run([str(executable)], check=True, capture_output=True, text=True)
            invalid = subprocess.run(command + ["-DZULE_CONFIG_TASK_NAMES=2"], capture_output=True, text=True)
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn("ZULE_CONFIG_TASK_NAMES must be 0 or 1", invalid.stderr)
            # Module storage is sized by the task count, and defined only when a
            # module keeps per-task data.
            self.assertIn("#if ZULE_MODULES_TASK_DATA", output.read_text())
            self.assertIn("zule_modules_task_data[3U];", output.read_text())
            original = output.read_bytes()
            header = Path(directory) / "zule_task_ids.h"
            original_header = header.read_bytes()
            self.assertEqual(generate_schedule(schedule, directory).read_bytes(), original)
            self.assertEqual(header.read_bytes(), original_header)
            reordered = load_schedule(FIXTURES / "valid/reordered-control-loop.json")
            self.assertEqual(generate_schedule(reordered, directory).read_bytes(), original)
            self.assertEqual(header.read_bytes(), original_header)

    def test_repeated_task_is_declared_once(self):
        schedule = load_schedule(FIXTURES / "valid/repeated-task.json")
        with tempfile.TemporaryDirectory() as directory:
            source = generate_schedule(schedule, directory).read_text()
            self.assertEqual(source.count("extern void zule_task_sense(void);"), 1)
            self.assertIn("{&zule_tasks[ZULE_TASK_ID_SENSE], 0U, 2U}", source)
            self.assertIn("{&zule_tasks[ZULE_TASK_ID_SENSE], 2U, 2U}", source)

    def test_task_references_and_unused_tasks(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler is required for generated-code integration test")
        for fixture in ("valid/repeated-task.json", "valid/unused-task.json"):
            with self.subTest(fixture=fixture), tempfile.TemporaryDirectory() as directory:
                schedule = load_schedule(FIXTURES / fixture)
                output = generate_schedule(schedule, directory)
                checks = []
                for index, name in enumerate(sorted(schedule["tasks"])):
                    task_id = f"ZULE_TASK_ID_{name.upper()}"
                    checks.append(f"    assert({task_id} == {index});")
                    checks.append(f'    assert(strcmp(zule_task_name_get({task_id}), "{name}") == 0);')
                for index, window in enumerate(schedule["cycle"]["windows"]):
                    task_id = f"ZULE_TASK_ID_{window['task'].upper()}"
                    checks.append(f"    assert(schedule->windows[{index}].task->id == {task_id});")
                if fixture.endswith("repeated-task.json"):
                    checks.append("    assert(schedule->windows[0].task == schedule->windows[1].task);")
                harness = Path(directory) / "test.c"
                harness.write_text('''#include <assert.h>
#include <string.h>
#include "zule_task_ids.h"
#include "zule_schedule_ids.h"
#include "zule_schedule_definition.h"
void zule_task_sense(void) {}
void zule_task_control(void) {}
int main(void) {
    assert(zule_schedules_validate(zule_schedules_get()) == ZULE_RESULT_OK);
    const zule_schedule_t *schedule = &zule_schedules_get()->schedules[ZULE_SCHEDULE_ID_MAIN];
''' + "\n".join(checks) + "\n    return 0;\n}\n")
                executable = Path(directory) / "test"
                subprocess.run([*compile_command(compiler, harness, output, executable), "-DZULE_CONFIG_TASK_NAMES=1"], check=True, capture_output=True, text=True)
                subprocess.run([str(executable)], check=True, capture_output=True, text=True)

    def test_multiple_schedules(self):
        """Named schedules take IDs in name order, with the shared task table and slot length."""
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler is required for generated-code integration test")
        schedule = load_schedule(FIXTURES / "valid/modes.json")
        with tempfile.TemporaryDirectory() as directory:
            output = generate_schedule(schedule, directory)
            header = (Path(directory) / "zule_schedule_ids.h").read_text()
            self.assertIn("ZULE_SCHEDULE_ID_OPERATIONAL = 0,", header)
            self.assertIn("ZULE_SCHEDULE_ID_STARTUP = 1,", header)
            harness = Path(directory) / "test.c"
            harness.write_text('''#include <assert.h>
#include "zule_task_ids.h"
#include "zule_schedule_ids.h"
#include "zule_schedule_definition.h"
void zule_task_sense(void) {}
void zule_task_control(void) {}
void zule_task_mode_manager(void) {}
int main(void) {
    const zule_schedules_t *schedules = zule_schedules_get();
    assert(schedules->schedule_count == 2U);
    assert(zule_schedules_validate(schedules) == ZULE_RESULT_OK);
    assert(schedules->slot_us == 1000U && schedules->task_count == 3U);
    const zule_schedule_t *operational = &schedules->schedules[ZULE_SCHEDULE_ID_OPERATIONAL];
    const zule_schedule_t *startup = &schedules->schedules[ZULE_SCHEDULE_ID_STARTUP];
    assert(operational->cycle_slots == 10U && operational->window_count == 3U);
    assert(startup->cycle_slots == 5U && startup->window_count == 1U);
    assert(startup->windows[0].task->id == ZULE_TASK_ID_MODE_MANAGER);
    assert(startup->windows[0].start_slot == 4U && startup->windows[0].budget_slots == 1U);
    assert(operational->windows[2].task->id == ZULE_TASK_ID_MODE_MANAGER);
    return 0;
}
''')
            executable = Path(directory) / "test"
            subprocess.run(compile_command(compiler, harness, output, executable), check=True, capture_output=True, text=True)
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)

    def test_invalid_input_preserves_existing_output(self):
        for name, message in (("invalid/overlap.json", "overlaps"), ("valid/large-slot.json", "uint32_t"), ("invalid/task-name-collision.json", "collides")):
            with self.subTest(fixture=name), tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "zule_schedule_definition.c"
                output.write_text("existing output")
                header = Path(directory) / "zule_task_ids.h"
                header.write_text("existing header")
                with self.assertRaisesRegex(ValueError, message):
                    generate_schedule(load_schedule(FIXTURES / name), directory)
                self.assertEqual(output.read_text(), "existing output")
                self.assertEqual(header.read_text(), "existing header")


if __name__ == "__main__":
    unittest.main()
