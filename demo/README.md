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
`main.c`, and links the `zule::core` library and selected port. Select
`-DZULE_PORT=posix` for the native build. `main.c` provides the tasks, configures
a fault handler, and calls the blocking public `zule_run()`; it has no timer code
or scheduler-state access. Its handler only copies the fault snapshot. Main
prints it after the runner returns, when tick delivery is quiescent, rather than
logging from tick context.

The runner initializes the scheduler and starts the port timer through a tick
callback. The POSIX port owns periodic delivery and main-loop idle behavior.
Tasks print their names and run outside critical sections. The cycle repeats
until a scheduler or port error; the runner stops and joins the tick thread
before returning, and the demo reports a nonzero exit. Press Ctrl-C to terminate
the demo process externally; there is no graceful public stop API yet.

CI performs a bounded smoke run, checks that all task names appear, then
terminates the process. Deterministic runner tests verify graceful error cleanup
separately. See [the POSIX port notes](../ports/posix/README.md).
