# Contributing

## Setup

Development needs a C compiler and Python 3.10 or later; the
[static checks](TESTING.md#static-checks) also need the Arm GNU toolchain and
Cppcheck, and the [target tests](TESTING.md#target-tests) the Arm GNU toolchain,
its C library, newlib, and QEMU. Set up either with the container image
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

Build Zule with the POSIX port and run its tests:

```sh
cd zule
cmake --preset tests
cmake --build --preset tests
ctest --preset tests
```

[TESTING.md](TESTING.md) describes every kind of test and check, how to run
each, and what CI runs.

To add a source file to the core or a module, list it in
[`zule/zule_sources.txt`](zule/zule_sources.txt). The Zule library, the core
tests, and the schedule tool's tests all read that one list. Each port lists its
own sources once in its `CMakeLists.txt`.

## Formatting

Format C sources with the pinned clang-format, independently of CMake:

```sh
python tools/format.py
python tools/format.py --check
```

Schedule JSON files have their own layout, which the
[schedule tool](zule/tools/schedule/README.md#formatting) applies:

```sh
cd zule/tools/schedule
python -m zule_schedule format --check path/to/schedule.json
```

## Coding guidelines

See [doc/CODING_GUIDELINES.md](doc/CODING_GUIDELINES.md) for naming, API,
ownership, and review conventions. [AGENTS.md](AGENTS.md) directs agent reviews
against those guidelines.

## Continuous integration

GitHub Actions builds the development image and runs every test and check in
it, for pushes and pull requests; see
[TESTING.md](TESTING.md#continuous-integration).
