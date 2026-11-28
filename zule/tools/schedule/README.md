# Zule schedule tools (draft)

A small Python package that validates [schedules](../../../doc/configuration.md#schedule),
shows them as a slot grid, formats them, and generates their C tables. CMake applications run it through
[`zule_add_schedule()`](../../../doc/application.md#cmake-integration); it can
also be run directly.

## Usage

Install `requirements.txt` (included in the repository's `requirements-dev.txt`),
then run from this directory:

```sh
python -m zule_schedule validate tests/fixtures/valid/control-loop.json
python -m zule_schedule show tests/fixtures/valid/control-loop.json
python -m zule_schedule format tests/fixtures/valid/control-loop.json
python -m zule_schedule generate tests/fixtures/valid/control-loop.json generated
```

The CLI returns 0 on success, 1 for invalid input or file errors, and 2 for
command usage errors. Validation errors go to stderr with a JSON Pointer
identifying the affected location (`/` denotes the document root).

## Viewing

`show <schedule.json>` lists the file's tasks, then renders each schedule as a
grid of slots:

```
tasks · 3 tasks
  control  ██████████████  3 slots
  output   █████████       2 slots
  sense    █████████       2 slots

schedule main · 10 slots · 1 ms · slot 100 us

           0    1    2    3    4    5    6    7    8    9
  control  ·    ·    ██████████████ ·    ·    ·    ·    ·
  output   ·    ·    ·    ·    ·    █████████ ·    ·    ·
  sense    █████████ ·    ·    ·    ·    ·    ·    ·    ·
  idle                                        ░░░░░░░░░░░░░░
           7/10 slots reserved · 70%
```

The tasks come first because a file's tasks are shared by every schedule in it,
and each schedule shows a lane only for the tasks it places. Tasks and
schedules are in the order of their generated IDs. A slot is always the same
width, so a task's budget is drawn exactly as its windows are; a cycle too long
for the terminal continues on the next stave, each labelled with its own slots.

The width comes from the terminal, or `--width`. Lanes are coloured when
standard output is a terminal, unless `--no-color` or `NO_COLOR` is set, and
each task keeps its colour throughout. The grid characters need a UTF-8
terminal.

A schedule that fails the [validation](#validation) checks is still rendered,
since seeing the problem is the point, and the issues are reported as for
`validate`. One whose structure is unusable cannot be drawn, so only its issues
are reported. Windows of one task that overlap are marked `▒`.

## Editing

`edit <schedule.json>` opens the schedule in the terminal, laid out as `show`
prints it: the file's tasks, which every schedule in it shares, then each
schedule below. One cursor moves over the whole of it with the arrow keys, and
`enter` acts on the cell it is on. What that cell offers is listed at the foot
of the screen, so there is nothing to remember and no mode to be in.

```
config/schedule.json

tasks
  control       3 slots   ██████████████
  mode_manager  1 slot    ████
  sense         2 slots   █████████
  + new task

schedule operational · 10 slots
                0    1    2    3    4    5    6    7    8    9
  control       ·    ·    ██████████████ ·    ·    ·    ·    ·
  mode_manager  ·    ·    ·    ·    ·    ·    ·    ·    ·    ████
  sense         █████████ ·    ·    ·    ·    ·    ·    ·    ·
  idle                                   ░░░░░░░░░░░░░░░░░░░

schedule startup · 5 slots
                0    1    2    3    4
  control       ·    ·    ·    ·    ·
  mode_manager  ·    ·    ·    ·    ████
  sense         ·    ·    ·    ·    ·
  idle          ░░░░░░░░░░░░░░░░░░░

  + new schedule

valid
  enter place mode_manager here  ↑↓←→ move  w write  q quit
```

A cell is one of these, and `enter` does what the key bar says:

| Cell | `enter` | `d` |
| --- | --- | --- |
| A task's name | Rename it. Its windows follow; the application's `zule_task_<name>()` has to be renamed to match. | Delete the task. If a schedule places it, `D` deletes it and those windows. The last task is kept. |
| A task's budget in slots | Set the budget. It belongs to the task, so this resizes its windows in every schedule, which can leave one overlapping. | As above. |
| `+ new task` | Add a task, asking for its name and budget. A name must be a C identifier whose uppercase form no other task takes, as the generated constants need. | |
| A schedule's name | Rename the schedule. | Delete it. `D` confirms when it holds windows. The last schedule is kept. |
| A schedule's length | Set the cycle length in slots. | As above. |
| An empty slot in a task's lane | Place a window for that lane's task there, or say why it will not fit. | |
| A slot inside a window | Pick the window up: `←→` then move it, `d` deletes it, `enter` puts it down. A held window is drawn whole, and marked where it cannot stay. | Delete the window, leaving its task in the file. |
| `+ new schedule` | Add a schedule, asking for its name. It starts with one window, since a cycle needs one. A file written as a single `cycle` becomes `schedules`, with the original named `main`. | |

Every schedule shows a lane for every task, dimmed where the task has no window
there, because a lane is where its windows are placed. Which task a new window
is for therefore needs no asking: it is the lane the cursor is in.

`w` writes the file in the layout [format](#formatting) writes, and `q` quits,
asking again if there is unsaved work; `Q` and a second Ctrl-C quit anyway.

The schedule is checked after every key, and the result is shown above the key
bar. An edit may leave a schedule the checks reject, since some edits need a gap
that does not exist yet; what is refused is writing one. The view scrolls to
keep the cursor in sight: down through the schedules, and across within one
whose cycle is longer than the terminal, each schedule keeping its own place.

The editor needs a terminal with `curses`, which the standard library provides
on Unix. The other commands do not import it, so an application build never
needs one.

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

## Formatting

`format <schedule.json>...` writes each schedule in the project's layout, and
`--check` reports the files that differ without writing any, returning 1 if
there are any. The [example schedules](../../../examples/README.md) are kept
formatted, which CI checks; schedules used as test data are not.

Formatting changes whitespace alone: keys keep the order they were read in, so
a file is never reordered. A value goes on one line where it fits in 80
columns, and is expanded over several where it does not, except the schedule
itself, which is always expanded. Writing a schedule the same way every time
keeps a timing change to a diff of the timing, and it is what will let the
editor save a file without rewriting the parts it did not touch.

## Development

```sh
python -m unittest discover -s tests -v
```

JSON test cases live in `tests/fixtures/`:

- `generic/`: reusable JSON values, such as `empty-object.json` and `null.json`.
- `valid/`: valid schedules, including the control-loop example.
- `invalid/`: schedules with specific structural, reference, ordering, or timing errors.
