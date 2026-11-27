"""Write a schedule in the project's JSON layout.

A value goes on one line where it fits and is expanded over several where it
does not, so that small schedules stay compact and long ones stay readable.
Keys keep the order they were read in, so formatting never reorders a file:
what it changes is whitespace alone.

Writing a schedule the same way every time keeps a timing change to a diff of
the timing, which matters for a file that is reviewed. It is also what lets the
planned editor save without rewriting the parts it did not touch.
"""

import json
from typing import Any

# Schedule files are read in review, so they keep to short lines as the C
# sources do, and are indented like the JSON already in the project.
_LINE_LIMIT = 80
_INDENT = 2


def format_schedule(schedule: Any) -> str:
    """Return the canonical text of a loaded schedule, ending with a newline."""
    return "\n".join(_lines(schedule, 0, "", expand=True)) + "\n"


def _lines(value: Any, indent: int, prefix: str, expand: bool = False) -> list[str]:
    """Return the lines for a value, given the text before it on its first line.

    The whole schedule is expanded whatever its length, since a schedule file
    written on one line would be hard to read and harder to diff.
    """
    inline = " " * indent + prefix + _inline(value)
    if len(inline) <= _LINE_LIMIT and not (expand and value):
        return [inline]

    if isinstance(value, dict):
        items = [(_scalar(key) + ": ", item) for key, item in value.items()]
    elif isinstance(value, list):
        items = [("", item) for item in value]
    else:
        # A scalar too long for the limit still has to go on its own line.
        return [inline]

    opening, closing = ("{", "}") if isinstance(value, dict) else ("[", "]")
    lines = [" " * indent + prefix + opening]
    for index, (item_prefix, item) in enumerate(items):
        item_lines = _lines(item, indent + _INDENT, item_prefix)
        if index < len(items) - 1:
            item_lines[-1] += ","
        lines.extend(item_lines)
    lines.append(" " * indent + closing)
    return lines


def _inline(value: Any) -> str:
    """Return a value on one line, spaced inside braces but not brackets."""
    if isinstance(value, dict):
        if not value:
            return "{}"
        members = ", ".join(f"{_scalar(key)}: {_inline(item)}" for key, item in value.items())
        return "{ " + members + " }"
    if isinstance(value, list):
        return "[" + ", ".join(_inline(item) for item in value) + "]"
    return _scalar(value)


def _scalar(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False)
