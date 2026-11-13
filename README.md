# Zule (draft)

A static, time-triggered scheduler with offline schedule validation and C table
generation.

See [the architecture overview](doc/architecture.md) for component boundaries,
application and tick contexts, and the relationship between the core and ports.

See [build configuration](doc/build_configuration.md) for compile-time defaults
and user overrides, including optional diagnostic task names. Task IDs are
generated from task names and are included in fault reports.

The layout follows the [ownership domains](doc/architecture.md#ownership):

- `zule/`: the Zule domain, added to an application with `add_subdirectory()`.
  - `core/`: platform-independent C scheduling engine; `core/api/` is public,
    `core/inc/` internal.
  - `ports/`: port API (`zule_port.h`) and native POSIX development port.
  - `tools/schedule/`: schedule validation and C generation, with tests and schema.
  - `cmake/`: `zule_add_schedule()` for generating schedules at build time.
- `demo/`: the user domain, an example application (`main.c`) and its
  configuration (`config/`). Also the reference CMake integration.
- `tools/format.py`: standalone C formatting utility for development.

Set up the Python tools from the project root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r zule/tools/schedule/requirements.txt
cd zule/tools/schedule
../../../.venv/bin/python -m zule_schedule validate tests/fixtures/valid/control-loop.json
../../../.venv/bin/python -m unittest discover -s tests -v
```

See [zule/tools/schedule/README.md](zule/tools/schedule/README.md) for the current format and tool behavior.

Build the Ubuntu demo with a C compiler, CMake (3.16 or later), and the Python
environment above. From the project root:

```sh
cmake -S demo -B build/demo -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build/demo
./build/demo/zule_demo
```

The build generates `build/demo/zule_demo_schedule/zule_schedule_definition.c`
and links it with the core and demo tasks. See [demo/README.md](demo/README.md). The demo calls the public blocking `zule_run()` entry point. The
runner initializes the scheduler, starts port-owned periodic ticks, and dispatches
tasks. The 500 ms cycle repeats with 50 ms slots until a fault; press Ctrl-C to
terminate the demo process. There is no public stop API yet. On an error return,
tick delivery has been stopped and quiesced. This native development example
provides no hard real-time guarantees or forced task termination.

Applications can call `zule_configure()` before running to install an optional
fault handler. Configuration is copied and cannot change while running. The
handler receives the first scheduler fault in tick context and must remain
bounded and nonblocking; see [the core notes](zule/core/README.md#application-fault-handler).

Applications add `zule/` with `add_subdirectory()`, select a port with
`ZULE_PORT`, link `zule::core`, and call `zule_add_schedule()`; see
[CMake integration](doc/build_configuration.md#cmake-integration). A core-only
build needs a C compiler and CMake, without Python. Integrators using another
build system compile the core and chosen port with their include paths and run
the schedule generator directly. See [zule/ports/posix/README.md](zule/ports/posix/README.md)
for the host port contract.

To build and run the core and port tests, with `zule/` as the top-level project:

```sh
cmake -S zule -B build/zule -DZULE_PORT=posix
cmake --build build/zule
ctest --test-dir build/zule --output-on-failure
```

For C development, install the pinned formatter into your virtual environment
and invoke it independently of CMake:

```sh
python -m pip install -r requirements-dev.txt
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
