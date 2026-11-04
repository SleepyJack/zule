# Agent instructions

Read [doc/CODING_GUIDELINES.md](doc/CODING_GUIDELINES.md) before changing or
reviewing C code, public headers, or the C generator. It is the reference for
project conventions; `.clang-format` is authoritative for layout.

Apply the guidelines strictly to the scheduler core, eventual ports, and
generated scheduler C. Demo application code should follow them where practical;
do not enforce strict compliance there. Generated code used by a demo remains
in scope.

## Guideline review

Checking the coding guidelines is primarily an agent responsibility. For each
relevant change, inspect the changed code and affected interfaces against the
current guidelines, including naming, header guards, linkage, const correctness,
integer types and units, and documented validation assumptions.

Fix deviations introduced by the change. Call out relevant remaining deviations
or assumptions in the final response, with file locations. Keep unrelated
cleanup separate. Do not claim full guideline compliance based only on a clean
formatter, successful build, or passing tests. Do not add approval steps for
routine convention fixes within the requested scope.

## Validation

Use the project's Python environment and the commands documented in
[README.md](README.md). Choose checks appropriate to the change:

- For C changes, run `python tools/format.py --check` and compile the affected
  targets. The formatter is independent of CMake.
- For schedule-tool changes, run `python -m unittest discover -s tests -v`
  from `tools/schedule/`.
- For generator or API changes, regenerate the schedule, check generated C
  formatting, and build/run the demo. Keep the public headers, generator,
  example JSON, and demo consistent.
- Fix generated-code issues in `tools/schedule/zule_schedule/generation.py`,
  not in `demo/gen/` or build output.
- Documentation-only changes need reference and consistency checks rather
  than a full build or test run.

State which checks were run and any limitations that affect the result.
