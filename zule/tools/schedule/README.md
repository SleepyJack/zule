# Zule schedule tools (draft)

A small Python package that validates [schedules](../../../doc/configuration.md#schedule)
and generates their C tables. CMake applications run it through
[`zule_add_schedule()`](../../../doc/application.md#cmake-integration); it can
also be run directly.

## Usage

Install `requirements.txt` (included in the repository's `requirements-dev.txt`),
then run from this directory:

```sh
python -m zule_schedule validate tests/fixtures/valid/control-loop.json
python -m zule_schedule generate tests/fixtures/valid/control-loop.json generated
```

The CLI returns 0 on success, 1 for invalid input or file errors, and 2 for
command usage errors. Validation errors go to stderr with a JSON Pointer
identifying the affected location (`/` denotes the document root).

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

- `zule_schedule_definition.c`, implementing `zule_schedule_get()` from the
  core's internal `zule_schedule_definition.h`;
- `zule_task_ids.h`, a standalone application header with the
  [task ID constants](../../../doc/configuration.md#task-names-and-ids).

All input is validated before either file is written, so validation failures
leave existing outputs untouched. Each file is staged before replacing its
destination, but replacement of the pair is not atomic: if writing fails, rerun
generation before compiling.

Generation requires all timing values to fit `uint32_t`; validation alone has no
width limit (`valid/large-slot.json` exercises this). Tasks and names are
emitted in task-name order, independently of JSON declaration order. Window
order is preserved. The output is deterministic, with no timestamps or input
paths. Names are always emitted, guarded by `ZULE_CONFIG_TASK_NAMES`, so
changing that setting needs no regeneration.

## Development

`schedule_io.py` loads JSON for all tools, `validation.py` checks schedules,
`generation.py` emits C, and `cli.py` handles arguments and output. Each has its
own test module:

```sh
python -m unittest discover -s tests -v
```

JSON test cases live in `tests/fixtures/`:

- `generic/`: reusable JSON values, such as `empty-object.json` and `null.json`.
- `valid/`: valid schedules, including the control-loop example.
- `invalid/`: schedules with specific structural, reference, ordering, or timing errors.

Generated formatting issues are fixed in `generation.py`, never in generated output.
