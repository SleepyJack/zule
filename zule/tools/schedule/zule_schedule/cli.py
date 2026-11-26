"""Command-line entry point for offline schedule tools."""

import argparse
import os
import shutil
import sys

from .generation import generate_schedule
from .rendering import render_schedule
from .schedule_io import load_schedule
from .validation import schema_issues, validate_schedule


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
    show = commands.add_parser("show", help="render a JSON schedule as a slot grid")
    show.add_argument("schedule", help="path to the JSON schedule")
    show.add_argument("--width", type=int, help="grid width in columns, default the terminal's")
    show.add_argument("--no-color", action="store_true", help="never colour the output")
    generate = commands.add_parser("generate", help="validate a JSON schedule and generate C, task IDs, and schedule IDs")
    generate.add_argument("schedule", help="path to the JSON schedule")
    generate.add_argument("output_dir", help="directory for the generated C and ID headers")
    args = parser.parse_args(argv)

    try:
        schedule = load_schedule(args.schedule)
    except (OSError, ValueError) as error:
        print(f"{args.schedule}: {error}", file=sys.stderr)
        return 1

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
