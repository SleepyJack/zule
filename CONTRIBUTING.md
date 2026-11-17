# Contributing

## Setup

Development needs a C compiler and Python 3.10 or later; the cross-compiled
builds also need the Arm GNU toolchain. Set up either with the container image
or natively.

### Container image

[`.devcontainer/Dockerfile`](.devcontainer/Dockerfile) defines an image with
every tool at a pinned version, and CI builds in it. Editors and tools that
support Dev Containers, such as VS Code, GitHub Codespaces, JetBrains IDEs, and
the `devcontainer` CLI, can open the repository in it through
[`.devcontainer/devcontainer.json`](.devcontainer/devcontainer.json). The image
also works directly with Docker or Podman, from the project root:

```sh
docker build -f .devcontainer/Dockerfile -t zule-dev .
docker run --rm -it --volume "$PWD:/work" --workdir /work zule-dev
```

### Native setup

`requirements-dev.txt` pins CMake, Ninja, clang-format, and the schedule tool's
dependencies. From the project root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

The commands below assume the environment is active, so `python`, `cmake`, and
`ctest` resolve to the pinned versions. Install the other tools from your
system. The image pins these versions, from Ubuntu 24.04:

| Tool | Version | Used for |
| --- | --- | --- |
| GCC | 13.3 | Host builds and tests |
| Arm GNU toolchain (`arm-none-eabi-gcc`) | 13.2.rel1 | Cross-compiled builds |
| Cppcheck | 2.13.0 | Static analysis (planned) |
| Python | 3.12 | The schedule tool |

## Building and testing

Builds use [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html),
which hold each configuration's settings. Each project defines its own, in its
`CMakePresets.json`, and build output goes to `build/` in the project root.

Build Zule with the POSIX port and run the core and port tests:

```sh
cd zule
cmake --preset tests
cmake --build --preset tests
ctest --preset tests
```

Build and run the demo, which also regenerates its schedule:

```sh
cd demo
cmake --preset demo
cmake --build --preset demo
../build/demo/zule_demo
```

Run the schedule tool's tests:

```sh
cd zule/tools/schedule
python -m unittest discover -s tests -v
```

To add a source file to the core or a module, list it in
[`zule/zule_sources.txt`](zule/zule_sources.txt). The Zule library, the core
tests, and the schedule tool's tests all read that one list. Each port lists its
own sources once in its `CMakeLists.txt`.

### Cross-compiled builds

The cross-compiled builds check the Zule library with the target compiler,
`arm-none-eabi-gcc`. They are a standalone project in
[`zule/tests/cross/`](zule/tests/cross), like the demo. Each links the library,
the [stub port](zule/ports/stub/README.md), and a generated schedule into a
small program that nothing runs: freestanding, with no C library, and with
warnings as errors, including `-Wconversion`, `-Wshadow`, and
`-Wstrict-prototypes`. Use of a C library function, or a warning with the
target's type sizes, fails the build. Its presets choose the core and the
features:

| Preset | Target | Features |
| --- | --- | --- |
| `m3` | Cortex-M3 (Armv7-M) | Defaults |
| `m3-all` | Cortex-M3 (Armv7-M) | All enabled |
| `m0plus` | Cortex-M0+ (Armv6-M) | Defaults |
| `m0plus-all` | Cortex-M0+ (Armv6-M) | All enabled |

```sh
cd zule/tests/cross
cmake --preset m3-all
cmake --build --preset m3-all
```

GCC requires `memcpy`, `memmove`, `memset`, and `memcmp` even in a freestanding
build, so the check program provides them, standing in for the target
environment.

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

GitHub Actions first builds the container image, caching its layers between
runs, and passes it to the other jobs, which run every step inside it. One job
runs the C and Python unit tests, checks handwritten and generated C
formatting, and builds and smoke-tests the demo; another runs the
cross-compiled builds. CI runs for pushes and pull requests, and the
[CI workflow](.github/workflows/ci.yml) can also be run manually.
