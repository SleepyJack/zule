# Zule demo

`control-loop.json` is a demo schedule maintained independently of the test
fixtures. It describes a 500 ms cycle with 50 ms slots for sensing, control,
output, and idle time. These deliberately slow slots suit ordinary host scheduling.

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
`main.c`, and links the `zule::core` library and POSIX port. Select
`-DZULE_PORT=posix` when configuring. The demo initializes before starting a timer
thread, which sleeps to absolute `CLOCK_MONOTONIC` deadlines and calls
`zule_tick()` while holding the port mutex. The main thread polls `zule_run()`
and prints task names; task execution takes place outside the critical section.

The timer delivers one cycle of ticks. The main loop drains the final published
task, stops the timer, and joins it before exiting. Mutex-protected status fields
carry errors between threads. Scheduler faults or timer errors cause a nonzero
exit. A timer delay of at least one slot causes a timer error rather than a burst
of catch-up ticks. The demo polls at 1 ms intervals and makes no hard real-time
guarantees. See [the POSIX port notes](../ports/posix/README.md).
