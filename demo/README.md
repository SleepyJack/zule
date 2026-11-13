# Zule demo

The demo is a user-domain application, separate from Zule itself, following the
[architecture](../doc/architecture.md#ownership):

- `main.c`: the application, providing task implementations and fault handling.
- `config/`: the configuration, consisting of the schedule `control-loop.json`
  and build settings `zule_user_config.h`.
- `CMakeLists.txt`: a standalone project and the reference
  [CMake integration](../doc/build_configuration.md#cmake-integration). It adds
  `../zule`, selects the POSIX port, and points Zule at `config/`.

`config/control-loop.json` is a demo schedule maintained independently of the test
fixtures. It describes a 500 ms cycle with 50 ms slots for sensing, control,
output, and idle time. These deliberately slow slots suit ordinary host scheduling.

To build and run the demo on Ubuntu, follow the CMake commands in the root
README. `zule_add_schedule()` generates the schedule in
`build/demo/zule_demo_schedule/` at build time, using the Python interpreter
selected by `Python3_EXECUTABLE`; install `zule/tools/schedule/requirements.txt`
into it first. CMake then compiles `main.c` and links the `zule::core` library
and selected port. Override the port with `-DZULE_PORT=<port>`. `main.c` provides the tasks, configures
a fault handler, and calls the blocking public `zule_run()`; it has no timer code
or scheduler-state access. Its handler only copies the fault snapshot. Main
prints it after the runner returns, when tick delivery is quiescent, rather than
logging from tick context.

The build exposes the generated `zule_task_ids.h` to demo sources for symbolic
IDs such as `ZULE_TASK_ID_SENSE`. IDs are assigned from zero in sorted task-name
order, so fault logs must be interpreted using the matching build or schedule.

Fault output includes the task ID and, when compiled in, its diagnostic name.
`config/zule_user_config.h` enables task names, overriding Zule's disabled
default. `ZULE_USER_CONFIG_DIR` applies this header to the core, port,
application, and generated code. Zule's own tests use the defaults. Edit the header or supply a compiler
definition to change the setting; see
[build configuration](../doc/build_configuration.md). Recompilation is required,
but schedule regeneration is not.

The runner initializes the scheduler and starts the port timer through a tick
callback. The POSIX port owns periodic delivery and main-loop idle behavior.
Tasks print their names and run outside critical sections. The cycle repeats
until a scheduler or port error; the runner stops and joins the tick thread
before returning, and the demo reports a nonzero exit. Press Ctrl-C to terminate
the demo process externally; there is no graceful public stop API yet.

CI performs a bounded smoke run, checks that all task names appear, then
terminates the process. Deterministic runner tests verify graceful error cleanup
separately. See [the POSIX port notes](../zule/ports/posix/README.md).
