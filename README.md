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
