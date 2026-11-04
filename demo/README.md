# Zule demo

`control-loop.json` is a demo schedule maintained independently of the test
fixtures. It describes a 1 ms cycle with sensing, control, output, and idle time.

With the virtual environment activated, run from the project root:

```sh
python demo/generate.py
```

The script can also be invoked from another working directory. It uses the
current Python interpreter and writes `demo/gen/zule_schedule_definition.c` on Windows and Linux.
Install `tools/schedule/requirements.txt` into that interpreter's environment first. The generated
directory is ignored by Git.

To build and run the demo on Ubuntu, follow the CMake commands in the root
README. CMake generates its own schedule in `build/demo/gen/`, compiles
`main.c`, and links the `zule::core` library. `main.c` initializes the scheduler
with `zule_init()`, then calls `zule_tick()` followed by `zule_run()` for each
slot in one cycle, checking each result code and printing the slot number and
task names. It reads the generated configuration only to determine cycle length;
no mutable scheduler state is exposed. Slots are logical
steps; the demo does not simulate ISR concurrency or wall-clock task execution.
