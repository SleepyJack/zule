# Zule schedule tools (draft)

A small Python package for offline schedule validation and C table generation.

From this directory:

```sh
python3 -m venv ../../../.venv
../../../.venv/bin/python -m pip install -r requirements.txt
../../../.venv/bin/python -m zule_schedule validate tests/fixtures/valid/control-loop.json
```

Alternatively, with dependencies installed, run from this directory:

```sh
python3 -m zule_schedule validate tests/fixtures/valid/control-loop.json
python3 -m zule_schedule generate tests/fixtures/valid/control-loop.json generated
python3 -m unittest discover -s tests -v
```

The CLI returns 0 on success, 1 for invalid input or file errors,
and 2 for command usage errors. Validation errors go to stderr with a JSON
Pointer identifying the affected location (`/` denotes the document root).

Validation first checks the bundled JSON Schema, then checks task names and references,
strictly increasing window starts, overlaps, and cycle bounds. Duplicate JSON
keys and nonstandard constants such as NaN are rejected when loading files.
Structural errors prevent semantic checks until the structure is corrected.

Each window occupies `[start_slot, start_slot + task.budget_slots)`. Windows
may touch, tasks may run multiple times, and gaps are idle time. Budgets include
the allowance for interrupt and scheduler interference within a window.

The task key serves as its diagnostic name and determines its generated enum
constant. Explicit numeric IDs are not part of the JSON format. For example:

```json
"tasks": {
  "sense": { "budget_slots": 2 },
  "control": { "budget_slots": 3 }
}
```

The schema lives in `zule_schedule/schedule.schema.json`; the control-loop fixture references
it directly. The CLI always uses this schema and does not fetch input `$schema`
references.

`schedule_io.py` loads JSON for all tools, `validation.py` checks schedules,
`generation.py` emits C, and `cli.py` handles arguments and output. Each has its own test
module. Run tools from this directory; distribution packaging can be added
if needed later.

JSON test cases live in `tests/fixtures/`:

- `generic/`: reusable JSON values, such as `empty-object.json`, `empty-array.json`, and `null.json`.
- `valid/`: valid schedules, including the control-loop example.
- `invalid/`: schedules with specific structural, reference, ordering, or timing errors.

`generate <schedule.json> <output_dir>` creates the output directory if needed
and writes `zule_schedule_definition.c` and `zule_task_ids.h`. All input is
validated before either output is written. Each file is staged before replacing
its destination; replacement of the pair is not atomic. If writing fails, rerun
generation successfully before compiling. Validation failures leave both outputs
untouched.

The generated C implements `zule_schedule_get()` from
`core/inc/zule_schedule_definition.h`, which includes the types in `zule_schedule.h`.
It returns a static schedule with cycle length, task definitions, windows,
and array counts.
The schedule's `slot_us` supplies the port timer period through the core runner.
Each `zule_task_t` definition holds its generated ID and function pointer, plus
a conditional `name` member when diagnostic names are enabled. Each window
holds a pointer to a task definition, start slot, and budget. Multiple windows
can reference the same definition. Applications provide `void zule_task_<name>(void)`
for each declared task. The prefix allows task identifiers such as `switch`
without colliding with C keywords. The generated file includes `zule_schedule_definition.h`
from `core/inc`; the internal scheduler initialization obtains the schedule through the getter.
Compile generated definitions with both `core/inc` and `core/api` on the include
path. Application-only sources need just the public `core/api` directory.
CMake applications call `zule_add_schedule()`, which runs this tool at build
time and sets these paths; see [build configuration](../../../doc/build_configuration.md#cmake-integration).
Only one generated schedule is intended per application at this stage.

Task definitions and names are emitted in task-key order, independently of JSON
declaration order. `zule_task_ids.h` is a standalone application header containing
`zule_task_id_t` and constants such as `ZULE_TASK_ID_CONTROL`. IDs start at zero
in sorted task-name order, including declared tasks without windows. The
generated C uses these constants for task IDs and task-array references. Names
must have distinct uppercase forms so enum constants cannot collide.

Applications can include `zule_task_ids.h` by adding the generation output
directory to their include path. IDs are local to a build: adding, removing, or
renaming a task may change other numeric IDs. Interpret saved fault logs with
the matching firmware version or schedule. The core still validates unique IDs
for runtime schedules, including handwritten definitions.

Names are compiled only when
`ZULE_CONFIG_TASK_NAMES=1`, with no generator option or regeneration required.
`zule_task_name_get(id)` returns the diagnostic name or `NULL` when unavailable.
See [build configuration](../../../doc/build_configuration.md) for defaults and
the optional `zule_user_config.h` override mechanism.

Generation requires all timing fields to fit `uint32_t`; validation alone has
no target-width limit (`valid/large-slot.json` exercises this distinction).
The generator preserves window order and gaps and emits immutable tables and a
getter, without timestamps or input paths. It does not multiply slot duration by cycle length;
the runtime must use suitable arithmetic for any such conversion.

This draft checks internal schedule consistency. It does not verify WCET,
hardware timer limits, task deadlines, or dependencies, and is not a qualified
safety tool.
