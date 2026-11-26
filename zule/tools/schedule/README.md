# Zule schedule tools (draft)

A small Python package that validates [schedules](../../../doc/configuration.md#schedule),
shows them as a slot grid, and generates their C tables. CMake applications run it through
[`zule_add_schedule()`](../../../doc/application.md#cmake-integration); it can
also be run directly.

## Usage

Install `requirements.txt` (included in the repository's `requirements-dev.txt`),
then run from this directory:

```sh
python -m zule_schedule validate tests/fixtures/valid/control-loop.json
python -m zule_schedule show tests/fixtures/valid/control-loop.json
python -m zule_schedule generate tests/fixtures/valid/control-loop.json generated
```

The CLI returns 0 on success, 1 for invalid input or file errors, and 2 for
command usage errors. Validation errors go to stderr with a JSON Pointer
identifying the affected location (`/` denotes the document root).

## Viewing

`show <schedule.json>` renders each schedule as a grid of slots, with one lane
per task in task ID order:

```
schedule main · 10 slots · 1 ms · slot 100 us

         0    1    2    3    4    5    6    7    8    9
control  ·    ·    ██████████████ ·    ·    ·    ·    ·      3 slots
output   ·    ·    ·    ·    ·    █████████ ·    ·    ·      2 slots
sense    █████████ ·    ·    ·    ·    ·    ·    ·    ·      2 slots
idle                                        ░░░░ ░░░░ ░░░░
         7/10 slots reserved · 70%
```

It fits the terminal, or `--width`, scaling the grid down for a long cycle, and
colours the lanes when standard output is a terminal, unless `--no-color` or
`NO_COLOR` is set. The grid characters need a UTF-8 terminal.

A schedule that fails the [validation](#validation) checks is still rendered,
since seeing the problem is the point, and the issues are reported as for
`validate`. One whose structure is unusable cannot be drawn, so only its issues
are reported. Windows of one task that overlap are marked `▒`.

## Validation

Loading rejects duplicate JSON keys and nonstandard constants such as NaN.
Validation then checks the bundled
[schema](zule_schedule/schedule.schema.json), followed by task names and
references, strictly increasing window starts, overlaps, and cycle bounds.
Structural errors prevent semantic checks until they are corrected. The tool
always uses its bundled schema and does not fetch a schedule's `$schema` reference.

This draft checks internal schedule consistency. It does not verify WCET,
hardware timer limits, task deadlines, or dependencies, and is not a qualified
safety tool.

## Generation

`generate <schedule.json> <output_dir>` creates the output directory if needed
and writes:

- `zule_schedule_definition.c`, implementing `zule_schedule_definition_get()`
  from the core's internal `zule_schedule_definition.h`;
- `zule_task_ids.h`, a standalone application header with the
  [task ID constants](../../../doc/configuration.md#task-names-and-ids);
- `zule_schedule_ids.h`, a standalone application header with the
  [schedule ID constants](../../../doc/configuration.md#multiple-schedules).

All input is validated before any file is written, so validation failures
leave existing outputs untouched.

Generation requires all timing values to fit `uint32_t`; validation alone has no
width limit (`valid/large-slot.json` exercises this).

## Development

```sh
python -m unittest discover -s tests -v
```

JSON test cases live in `tests/fixtures/`:

- `generic/`: reusable JSON values, such as `empty-object.json` and `null.json`.
- `valid/`: valid schedules, including the control-loop example.
- `invalid/`: schedules with specific structural, reference, ordering, or timing errors.
