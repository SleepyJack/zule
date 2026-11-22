# Contributing

## Setup

Development needs a C compiler and Python 3.10 or later; the cross-compiled
builds also need the Arm GNU toolchain. Set up either with the container image
or natively.

### Container image

The [development image](.devcontainer/README.md) has every tool at a pinned
version, and CI builds in it. Use it with Docker or Podman, or open the
repository in it with an editor that supports Dev Containers.

### Native setup

`requirements-dev.txt` pins CMake, Ninja, clang-format, and the schedule tool's
dependencies. From the project root:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

The commands below assume the environment is active, so `python`, `cmake`, and
`ctest` resolve to the pinned versions. Install the other tools, such as GCC
and the Arm GNU toolchain, from your system;
[the Dockerfile](.devcontainer/Dockerfile) lists the versions CI uses.

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

The `sanitize` preset builds and runs the same tests under
UndefinedBehaviorSanitizer and AddressSanitizer, in `build/zule-sanitize/`. Any
undefined behaviour or invalid memory access they detect fails the test:

```sh
cd zule
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize
```

Build and run an [example](examples/README.md), which also regenerates its
schedule. Each example has a preset for each target it supports, and builds to
`build/examples/<example>/<target>/`:

```sh
cd examples/showcase
cmake --preset posix
cmake --build --preset posix
../../build/examples/showcase/posix/zule_showcase
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
[`zule/tests/cross/`](zule/tests/cross), like the examples. Each links the library,
a port, and a generated schedule into a small program that nothing runs:
freestanding, with no C library, and with warnings as errors, including
`-Wconversion`, `-Wshadow`, and `-Wstrict-prototypes`. Use of a C library
function, or a warning with the target's type sizes, fails the build. Its
presets, named `<port>-<core>[-all]`, choose the port, the core, and the
features:

| Preset | Port | Core | Features |
| --- | --- | --- | --- |
| `stub-m3` | [Stub](zule/ports/stub/README.md) | Cortex-M3 (Armv7-M) | Defaults |
| `stub-m3-all` | Stub | Cortex-M3 (Armv7-M) | All enabled |
| `stub-m0plus` | Stub | Cortex-M0+ (Armv6-M) | Defaults |
| `stub-m0plus-all` | Stub | Cortex-M0+ (Armv6-M) | All enabled |
| `cortex-m-m3` | [Cortex-M](zule/ports/cortex_m/README.md) | Cortex-M3 (Armv7-M) | Defaults |
| `cortex-m-m0plus` | Cortex-M | Cortex-M0+ (Armv6-M) | Defaults |

The stub port checks the Zule library alone; a real port's presets check it with
the library. The Cortex-M port has no all-enabled presets until it provides the
timebase, which the other features need.

```sh
cd zule/tests/cross
cmake --preset stub-m3-all
cmake --build --preset stub-m3-all
```

GCC requires `memcpy`, `memmove`, `memset`, and `memcmp` even in a freestanding
build, so the check program provides them, standing in for the target
environment.

### Static analysis

After compiling, the cross-compiled builds run
[Cppcheck](https://cppcheck.sourceforge.io/) and its MISRA C:2012 addon over the
Zule library, the selected port, and the generated schedule, analysed together
as one program from the build's compile database. It checks for errors,
warnings, style, and portability problems, assuming 32-bit types as on
Cortex-M, for functions that are never used, and for violations of the MISRA
rules the addon covers; see
[MISRA C](doc/CODING_GUIDELINES.md#misra-c). A finding fails the build. Fix the
code rather than suppress the finding; see
[static analysis findings](doc/CODING_GUIDELINES.md#static-analysis-findings).

## Formatting

Format C sources with the pinned clang-format, independently of CMake:

```sh
python tools/format.py
python tools/format.py --check
```

By default, the script formats handwritten C sources and headers under `zule/`
and `examples/`. `--check` reports differences without editing and returns a failure
status if formatting is needed. Paths are resolved from the script, so it can
be invoked from any working directory. Explicit file arguments are relative
to the caller's working directory.

Generated files are excluded by default. After building an example, check its
generated schedule explicitly:

```sh
python tools/format.py --check build/examples/showcase/posix/zule_showcase_schedule/*.[ch]
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
formatting, and builds and runs the examples; another runs the C unit tests
under the sanitizers; and another runs the cross-compiled builds. CI runs for
pushes and pull requests, and the [CI workflow](.github/workflows/ci.yml) can
also be run manually.
