# Zule (draft)

A static, time-triggered scheduler with offline schedule validation and C table
generation.

- `core/`: platform-independent C scheduling engine and public interfaces.
- `tools/`: Python tools, their tests, JSON fixtures, and schema.
- `demo/`: a standalone example schedule and generation script.

Set up the Python tools from the project root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
cd tools
../.venv/bin/python -m zule_tools validate tests/fixtures/valid/control-loop.json
../.venv/bin/python -m unittest discover -s tests -v
```

Generate the demo from the project root with your virtual environment activated:

```sh
python demo/generate.py
```

Output goes to `demo/gen/schedule.c`. See [demo/README.md](demo/README.md).

See [tools/README.md](tools/README.md) for the current format and tool behavior.

Build the Ubuntu demo with a C compiler, CMake (3.16 or later), and the Python
environment above. From the project root:

```sh
cmake -S . -B build -DZULE_BUILD_DEMO=ON -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
cmake --build build
./build/demo/zule_demo
```

The build generates `build/demo/gen/schedule.c` and links it with the core and
demo tasks. The executable steps through one cycle of logical slots; it does
not yet implement wall-clock timing or forced termination of overrunning tasks.

`ZULE_BUILD_DEMO` defaults to `OFF`. A core-only build needs a C compiler and
CMake, without Python. Consumers can use `add_subdirectory(path/to/zule)` and
link `zule::core`, or compile `core/src/zule.c` with `core/inc` on their include path.
