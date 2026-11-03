"""Command-line entry point for offline schedule tools."""

import argparse
import sys

from .generation import generate_schedule
from .schedule_io import load_schedule
from .validation import validate_schedule


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="zule-tools")
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate", help="validate a JSON schedule")
    validate.add_argument("schedule", help="path to the JSON schedule")
    generate = commands.add_parser("generate", help="validate a JSON schedule and generate schedule.c")
    generate.add_argument("schedule", help="path to the JSON schedule")
    generate.add_argument("output_dir", help="directory for the generated schedule.c")
    args = parser.parse_args(argv)

    try:
        schedule = load_schedule(args.schedule)
    except (OSError, ValueError) as error:
        print(f"{args.schedule}: {error}", file=sys.stderr)
        return 1

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
        return 0

    issues = validate_schedule(schedule)
    for issue in issues:
        print(f"{args.schedule}:{issue.path}: {issue.message}", file=sys.stderr)
    if issues:
        return 1
    print(f"{args.schedule}: valid")
    return 0
