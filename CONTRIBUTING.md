# Contributing

## Setup

Development needs a C compiler and Python 3.10 or later. Everything else is
pinned in `requirements-dev.txt`: CMake, Ninja, clang-format, and the schedule
tool's dependencies. From the project root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

The commands below assume the environment is active, so `python`, `cmake`, and
`ctest` resolve to the pinned versions.

## Building and testing

Build Zule as the top-level project to build the core and port tests:

```sh
cmake -S zule -B build/zule -G Ninja -DZULE_PORT=posix
cmake --build build/zule
ctest --test-dir build/zule --output-on-failure
```

Build and run the demo, which also regenerates its schedule:

```sh
cmake -S demo -B build/demo -G Ninja
cmake --build build/demo
./build/demo/zule_demo
```

Run the schedule tool's tests:

```sh
cd zule/tools/schedule
python -m unittest discover -s tests -v
```

## Formatting

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

Generated formatting fixes belong in the Python generator, never in generated
output.

## Coding guidelines

The C style uses four spaces, attached braces, a 100-column limit, and pointer
stars next to variable names. All conditional and loop bodies use braces.
See [doc/CODING_GUIDELINES.md](doc/CODING_GUIDELINES.md) for naming, API,
ownership, and review conventions. [AGENTS.md](AGENTS.md) directs agent reviews
against those guidelines.

## Continuous integration

GitHub Actions runs the C and Python unit tests, checks handwritten and
generated C formatting, and builds and smoke-tests the demo on Ubuntu for pushes
and pull requests. The [CI workflow](.github/workflows/ci.yml) can also be run
manually.
