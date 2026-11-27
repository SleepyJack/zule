"""Command-line entry point for offline schedule tools."""

import argparse
import os
import shutil
import sys
from pathlib import Path

from .formatting import format_schedule
from .generation import generate_schedule
from .rendering import render_schedule
from .schedule_io import load_schedule
from .validation import schema_issues, validate_schedule


def _format(paths: list[str], check: bool) -> int:
    """Write each schedule in the canonical layout, or report those that differ."""
    status = 0
    for path in paths:
        try:
            original = Path(path).read_text(encoding="utf-8")
            formatted = format_schedule(load_schedule(path))
        except (OSError, ValueError) as error:
            print(f"{path}: {error}", file=sys.stderr)
            status = 1
            continue
        if formatted == original:
            continue
        if check:
            print(f"{path}: needs formatting", file=sys.stderr)
            status = 1
            continue
        try:
            Path(path).write_text(formatted, encoding="utf-8")
        except OSError as error:
            print(f"{path}: {error}", file=sys.stderr)
            status = 1
            continue
        print(f"{path}: formatted")
    return status


def _show(path: str, schedule: object, width: int | None, no_color: bool) -> int:
    """Render the schedule, then report any semantic issues it still has.

    A schedule that fails the structural checks cannot be rendered, so only its
    issues are reported.
    """
    issues = schema_issues(schedule)
    if not issues:
        if width is None:
            width = min(shutil.get_terminal_size(fallback=(100, 24)).columns, 160)
        color = not no_color and not os.environ.get("NO_COLOR") and sys.stdout.isatty()
        print(render_schedule(schedule, width=width, color=color))
        issues = validate_schedule(schedule)
    for issue in issues:
        print(f"{path}:{issue.path}: {issue.message}", file=sys.stderr)
    return 1 if issues else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="zule-schedule")
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate", help="validate a JSON schedule")
    validate.add_argument("schedule", help="path to the JSON schedule")
    formatting = commands.add_parser("format", help="write JSON schedules in the project's layout")
    formatting.add_argument("schedules", nargs="+", help="paths to JSON schedules")
    formatting.add_argument(
        "--check", action="store_true", help="report files that need formatting, writing none"
    )
    edit = commands.add_parser("edit", help="edit a JSON schedule in the terminal")
    edit.add_argument("schedule", help="path to the JSON schedule")
    show = commands.add_parser("show", help="render a JSON schedule as a slot grid")
    show.add_argument("schedule", help="path to the JSON schedule")
    show.add_argument("--width", type=int, help="grid width in columns, default the terminal's")
    show.add_argument("--no-color", action="store_true", help="never colour the output")
    generate = commands.add_parser("generate", help="validate a JSON schedule and generate C, task IDs, and schedule IDs")
    generate.add_argument("schedule", help="path to the JSON schedule")
    generate.add_argument("output_dir", help="directory for the generated C and ID headers")
    args = parser.parse_args(argv)

    if args.command == "format":
        return _format(args.schedules, args.check)

    try:
        schedule = load_schedule(args.schedule)
    except (OSError, ValueError) as error:
        print(f"{args.schedule}: {error}", file=sys.stderr)
        return 1

    if args.command == "edit":
        issues = schema_issues(schedule)
        for issue in issues:
            print(f"{args.schedule}:{issue.path}: {issue.message}", file=sys.stderr)
        if issues:
            return 1
        # Imported here so that the other commands, which every application
        # build runs, do not need a terminal library.
        from .editor import run

        return run(args.schedule, schedule)

    if args.command == "show":
        return _show(args.schedule, schedule, args.width, args.no_color)

    if args.command == "generate":
        try:
            output = generate_schedule(schedule, args.output_dir)
        except ValueError as error:
            print(f"{args.schedule}: {error}", file=sys.stderr)
            return 1
        except OSError as error:
            print(f"{args.output_dir}: {error}", file=sys.stderr)
            return 1
        print(f"{output}: generated")
        print(f"{output.with_name('zule_task_ids.h')}: generated")
        print(f"{output.with_name('zule_schedule_ids.h')}: generated")
        return 0

    issues = validate_schedule(schedule)
    for issue in issues:
        print(f"{args.schedule}:{issue.path}: {issue.message}", file=sys.stderr)
    if issues:
        return 1
    print(f"{args.schedule}: valid")
    return 0
