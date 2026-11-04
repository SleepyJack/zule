# Zule schedule tools (draft)

A small Python package for offline schedule validation and C table generation.

From this directory:

```sh
python3 -m venv ../../.venv
../../.venv/bin/python -m pip install -r requirements.txt
../../.venv/bin/python -m zule_schedule validate tests/fixtures/valid/control-loop.json
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

Validation first checks the bundled JSON Schema, then checks task references,
strictly increasing window starts, overlaps, and cycle bounds. Duplicate JSON
keys and nonstandard constants such as NaN are rejected when loading files.
Structural errors prevent semantic checks until the structure is corrected.

Each window occupies `[start_slot, start_slot + task.budget_slots)`. Windows
may touch, tasks may run multiple times, and gaps are idle time. Budgets include
the allowance for interrupt and scheduler interference within a window.

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
and writes `zule_schedule.c`, replacing existing output only after the new file is
complete. Validation failures leave existing output untouched.

The generated C implements `zule_schedule_get()` from `core/inc/zule_schedule.h`.
It returns a static schedule with cycle length, windows, and window count.
`zule_slot_us` is emitted separately as timing metadata; the core schedule type
does not contain it. Each window holds a function
pointer, start slot, and budget. Applications provide `void zule_task_<name>(void)`
for each scheduled task. The prefix allows task identifiers such as `switch`
without colliding with C keywords. The generated file includes `zule_schedule.h`
from `core/inc`; `zule_init()` obtains the schedule through the getter.
Only one generated schedule is intended per application at this stage.

Generation requires all timing fields to fit `uint32_t`; validation alone has
no target-width limit (`valid/large-slot.json` exercises this distinction).
The generator preserves window order and gaps and emits no runtime code,
timestamps, or input paths. It does not multiply slot duration by cycle length;
the runtime must use suitable arithmetic for any such conversion.

This draft checks internal schedule consistency. It does not verify WCET,
hardware timer limits, task deadlines, or dependencies, and is not a qualified
safety tool.
