# Zule demo

`control-loop.json` is a demo schedule maintained independently of the test
fixtures. It describes a 1 ms cycle with sensing, control, output, and idle time.

With the virtual environment activated, run from the project root:

```sh
python demo/generate.py
```

The script can also be invoked from another working directory. It uses the
current Python interpreter and writes `demo/gen/schedule.c` on Windows and Linux.
Install `tools/requirements.txt` into that interpreter's environment first. The generated
directory is ignored by Git.

To build and run the demo on Ubuntu, follow the CMake commands in the root
README. CMake generates its own schedule in `build/demo/gen/`, compiles
`main.c`, and links the `zule::core` library. `main.c` calls `zule_tick()` once
for each slot in the cycle, printing the slot number and task names. Slots are
logical steps in this skeleton; there is no real-time clock or overrun handling.
