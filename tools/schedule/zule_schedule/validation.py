"""Structural and semantic checks for the draft schedule format."""

import json
from dataclasses import dataclass
from importlib.resources import files
from typing import Any

from jsonschema import Draft202012Validator


@dataclass(frozen=True)
class ValidationIssue:
    path: str
    message: str


def _pointer(parts: Any) -> str:
    return "".join(
        "/" + str(part).replace("~", "~0").replace("/", "~1")
        for part in parts
    ) or "/"


def validate_schedule(schedule: Any) -> list[ValidationIssue]:
    """Return issues, checking semantics only after structural validation passes.

    The bundled schema is authoritative; input $schema values are editor hints
    and are never fetched. Budgets are declarations, not measured WCET bounds.
    """
    schema = json.loads(
        files("zule_schedule").joinpath("schedule.schema.json").read_text(encoding="utf-8")
    )
    validator = Draft202012Validator(schema)
    errors = sorted(
        validator.iter_errors(schedule),
        key=lambda error: (_pointer(error.absolute_path), error.message),
    )
    issues = [
        ValidationIssue(_pointer(error.absolute_path), error.message)
        for error in errors
    ]
    if issues:
        return issues

    tasks = schedule["tasks"]
    enum_names = {}
    for name in tasks:
        enum_name = name.upper()
        if enum_name in enum_names:
            issues.append(ValidationIssue(
                _pointer(("tasks", name)),
                f"task name collides with {enum_names[enum_name]!r} in generated enum constants",
            ))
        else:
            enum_names[enum_name] = name
    cycle = schedule["cycle"]
    previous_start = None
    occupied_until = 0
    occupied_by = None
    for index, window in enumerate(cycle["windows"]):
        path = f"/cycle/windows/{index}"
        start = window["start_slot"]
        if previous_start is not None and start <= previous_start:
            issues.append(ValidationIssue(
                path + "/start_slot", "windows must have strictly increasing start slots"
            ))
        previous_start = start
        if start >= cycle["slots"]:
            issues.append(ValidationIssue(
                path + "/start_slot", "window starts outside the cycle"
            ))
        task = window["task"]
        if task not in tasks:
            issues.append(ValidationIssue(path + "/task", f"unknown task {task!r}"))
            continue
        end = start + tasks[task]["budget_slots"]
        if end > cycle["slots"]:
            issues.append(ValidationIssue(
                path, f"window ends at slot {end}, beyond cycle end {cycle['slots']}"
            ))
        # Comparing against the furthest prior end also catches nested overlaps.
        if occupied_by is not None and start < occupied_until:
            issues.append(ValidationIssue(path, f"window overlaps window {occupied_by}"))
        if end > occupied_until:
            occupied_until = end
            occupied_by = index
    return issues
