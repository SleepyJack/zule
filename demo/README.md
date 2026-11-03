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
directory is ignored by Git. This demo currently generates the schedule table;
it does not execute a scheduler.
