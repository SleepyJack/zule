# Zule (draft)

A static, time-triggered scheduler with offline schedule validation and C table
generation.

## Documentation

The documentation follows the components of the [architecture](doc/architecture.md):

| Document | Covers |
| --- | --- |
| [Architecture](doc/architecture.md) | Components, ownership, and execution contexts. |
| [Configuration](doc/configuration.md) | Schedule format, task names and IDs, and build settings. |
| [Application](doc/application.md) | Tasks, the public API, fault handling, and integrating Zule into a build. |
| [Core](doc/core.md) | Scheduler behaviour, timing, faults, and implementation notes. |
| [Port](doc/port.md) | The port contract, and creating and linking a custom port. |

## Layout

The layout follows the [ownership domains](doc/architecture.md#ownership):

- `zule/`: the Zule domain, added to an application with `add_subdirectory()`.
  - `core/`: platform-independent C scheduling engine; `core/api/` is public,
    `core/inc/` internal.
  - `ports/`: port API (`zule_port.h`) and [POSIX development port](zule/ports/posix/README.md).
  - `tools/schedule/`: [schedule validation and C generation](zule/tools/schedule/README.md).
  - `cmake/`: `zule_add_schedule()` for generating schedules at build time.
- `demo/`: the user domain, an [example application](demo/README.md) and its
  configuration. Also the reference CMake integration.
- `tools/format.py`: standalone C formatting utility for development.

## Development

Development needs a C compiler and Python 3.10 or later. Everything else is
pinned in `requirements-dev.txt`: CMake, Ninja, clang-format, and the schedule
tool's dependencies. Set up and activate a virtual environment from the project root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

The commands below assume the environment is active, so `python`, `cmake`, and
`ctest` resolve to the pinned versions. Integrators need only CMake 3.31 or later,
plus `zule/tools/schedule/requirements.txt` when generating schedules.

Build and run the demo:

```sh
cmake -S demo -B build/demo -G Ninja
cmake --build build/demo
./build/demo/zule_demo
```

To build and run the core and port tests, with `zule/` as the top-level project:

```sh
cmake -S zule -B build/zule -G Ninja -DZULE_PORT=posix
cmake --build build/zule
ctest --test-dir build/zule --output-on-failure
```

To run the schedule tool's tests:

```sh
cd zule/tools/schedule
python -m unittest discover -s tests -v
```

Format C sources with the pinned clang-format, independently of CMake:

```sh
python tools/format.py
python tools/format.py --check
```

By default, the script formats handwritten C sources and headers under `zule/`
and `demo/`. `--check` reports differences without editing and returns a failure
status if formatting is needed. Paths are resolved from the script, so it can
be invoked from any working directory. Explicit file arguments are relative
to the caller's working directory.

Generated files are excluded by default. After building the demo, check its
generated schedule explicitly:

```sh
python tools/format.py --check build/demo/zule_demo_schedule/zule_schedule_definition.c build/demo/zule_demo_schedule/zule_task_ids.h
```

Generated formatting fixes belong in the Python generator. Formatting requires
no CMake configuration or compilation and is optional for integrators.

The C style uses four spaces, attached braces, a 100-column limit, and pointer
stars next to variable names. All conditional and loop bodies use braces.

See [doc/CODING_GUIDELINES.md](doc/CODING_GUIDELINES.md) for naming, API,
ownership, and review conventions. [AGENTS.md](AGENTS.md) directs agent reviews
against those guidelines.

GitHub Actions runs the C and Python unit tests, checks handwritten and
generated C formatting, and builds/smoke-tests the demo on Ubuntu for pushes and pull
requests. The [CI workflow](.github/workflows/ci.yml) can also be run manually.
