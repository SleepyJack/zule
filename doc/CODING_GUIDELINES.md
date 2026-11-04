# C coding guidelines

These conventions apply to the Zule scheduler (`core/` and eventual ports) and
to generated scheduler C, wherever that output is used. Demo application code
should follow them where practical, but strict adherence is not required.

Apply them to new and changed code; report relevant existing deviations without
expanding the change into an unrelated cleanup. Update this document as decisions evolve.

## Formatting

[.clang-format](../.clang-format) is authoritative for layout. The style uses
four spaces, no tabs, attached opening braces, a 100-column limit, and pointer
stars next to variable names:

```c
const zule_window_t *window;

if (window != NULL) {
    window->task_function();
}
```

Use braces for every conditional and loop body. Run the standalone formatter
from the repository root with the development environment active:

```sh
python tools/format.py
python tools/format.py --check
```

Generated files are excluded by default. Check them explicitly and fix their
generator rather than editing generated output:

```sh
python tools/format.py --check build/demo/gen/zule_schedule.c
```

## Naming and files

| Item | Convention | Example |
| --- | --- | --- |
| Project-defined types | `zule_..._t` | `zule_schedule_t` |
| Public functions and global symbols | `zule_...` | `zule_tick()` |
| Enum constants and macros | `ZULE_...` | `ZULE_TASK_PENDING` |
| Public headers | `zule_....h`; main header is `zule.h` | `zule_schedule.h` |
| Scheduler source files | `zule_....c`, matching the component | `zule_scheduler.c`, `zule_schedule.c` |
| Header guards | Uppercase filename with dots replaced by underscores | `ZULE_SCHEDULE_H` |
| Internal functions and objects | Descriptive snake case, with `static` linkage | `advance_window()` |
| Locals, parameters, and members | Descriptive snake case | `active_window` |

## Interfaces and visibility

- Export only intentional API symbols; give implementation-only functions and
  file-scope objects `static` linkage.
- Use `const` for access to immutable data.

## Integers, units, and initialization

- Use fixed-width integer types where the representation or range is part of
  the design contract, such as timing quantities, stored formats, and hardware
  interfaces. Use `size_t` for in-memory object sizes, array counts, and array
  indexes.
- Make units explicit in names, for example `start_slot`, `budget_slots`, and
  `slot_us`. Document any conversions and their overflow assumptions.
- State validation assumptions explicitly, including nonzero divisors, valid
  array indexes, and the lifetime of referenced configuration data.
