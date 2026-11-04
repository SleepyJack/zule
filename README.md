# Zule (draft)

A static, time-triggered scheduler with offline schedule validation and C table
generation.

- `core/`: platform-independent C scheduling engine and public interfaces.
- `tools/schedule/`: schedule definition tools, their tests, fixtures, and schema.
- `tools/format.py`: standalone C formatting utility.
- `demo/`: a standalone example schedule and generation script.

Set up the Python tools from the project root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/schedule/requirements.txt
cd tools/schedule
../../.venv/bin/python -m zule_schedule validate tests/fixtures/valid/control-loop.json
../../.venv/bin/python -m unittest discover -s tests -v
```

Generate the demo from the project root with your virtual environment activated:

```sh
python demo/generate.py
```

Output goes to `demo/gen/zule_schedule_definition.c`. See [demo/README.md](demo/README.md).

See [tools/schedule/README.md](tools/schedule/README.md) for the current format and tool behavior.

Build the Ubuntu demo with a C compiler, CMake (3.16 or later), and the Python
environment above. From the project root:

```sh
cmake -S . -B build -DZULE_BUILD_DEMO=ON -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build
./build/demo/zule_demo
```

The build generates `build/demo/gen/zule_schedule_definition.c` and links it with the core and
demo tasks. The executable steps through one cycle of logical slots; it does
not yet implement wall-clock timing or forced termination of overrunning tasks.

`ZULE_BUILD_DEMO` defaults to `OFF`. A core-only build needs a C compiler and
CMake, without Python. Consumers can use `add_subdirectory(path/to/zule)` and
link `zule::core`, or compile the C sources in `core/src/` with `core/inc` on their include path.

To build and run the core API tests:

```sh
cmake -S . -B build -DZULE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

For C development, install the pinned formatter into your virtual environment
and invoke it independently of CMake:

```sh
python -m pip install -r requirements-dev.txt
python tools/format.py
python tools/format.py --check
```

By default, the script formats handwritten C sources and headers under `core/`
and `demo/`. `--check` reports differences without editing and returns a failure
status if formatting is needed. Paths are resolved from the script, so it can
be invoked from any working directory. Explicit file arguments are relative
to the caller's working directory.

Generated files are excluded by default. After building the demo, check its
generated schedule explicitly:

```sh
python tools/format.py --check build/demo/gen/zule_schedule_definition.c
```

Generated formatting fixes belong in the Python generator. Formatting requires
no CMake configuration or compilation and is optional for integrators.

The C style uses four spaces, attached braces, a 100-column limit, and pointer
stars next to variable names. All conditional and loop bodies use braces.

See [doc/CODING_GUIDELINES.md](doc/CODING_GUIDELINES.md) for naming, API,
ownership, and review conventions. [AGENTS.md](AGENTS.md) directs agent reviews
against those guidelines.
