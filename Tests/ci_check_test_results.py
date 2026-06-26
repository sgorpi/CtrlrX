#!/usr/bin/env python3
"""Evaluate GoogleTest JUnit XML for CI checks.

Reads one or more JUnit result files (gtest --gtest_output=xml / GTEST_OUTPUT) and
decides pass/fail. With --allow <file>, failures whose fully-qualified name is listed in
that allowlist are tolerated (used by the "Tests (excluding known failures)" check);
without it, any failure fails the run (the "Tests" check).

A test's fully-qualified name is its JUnit ``classname`` + "." + ``name`` -- the same
string gtest prints after "[ FAILED ]".

Exit code: 0 if no unexpected failures, 1 otherwise (also 1 if no results were found).
"""
import argparse
import glob
import os
import sys
import xml.etree.ElementTree as ET


def load_allowlist(path):
    allowed = set()
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                allowed.add(line)
    return allowed


def collect(paths):
    """Return (all_names, failing_names) across every result file found."""
    all_names, failing = set(), set()
    files = []
    for pattern in paths:
        files.extend(glob.glob(pattern, recursive=True))
    for path in sorted(set(files)):
        root = ET.parse(path).getroot()
        for case in root.iter("testcase"):
            fqn = "{}.{}".format(case.get("classname", ""), case.get("name", ""))
            all_names.add(fqn)
            if case.find("failure") is not None or case.find("error") is not None:
                failing.add(fqn)
    return all_names, failing, files


def emit(text):
    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as fh:
            fh.write(text + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allow", metavar="FILE",
                        help="file of allowlisted fully-qualified test names")
    parser.add_argument("results", nargs="+",
                        help="JUnit XML files or globs (** supported)")
    args = parser.parse_args()

    allowed = load_allowlist(args.allow) if args.allow else set()
    all_names, failing, files = collect(args.results)

    if not files:
        emit("::error::No test result files matched: {}".format(" ".join(args.results)))
        return 1

    unexpected = sorted(failing - allowed)
    # Allowlisted names that ran and passed -> the list is stale.
    stale = sorted((allowed & all_names) - failing)

    emit("### {}".format("Tests (excluding known failures)" if args.allow else "Tests"))
    emit("- result files: {}".format(len(files)))
    emit("- testcases: {}".format(len(all_names)))
    emit("- failing: {} ({} allowlisted)".format(len(failing), len(failing & allowed)))

    for name in stale:
        emit("::warning::Allowlisted test now passes, remove from known_failures: {}".format(name))

    if unexpected:
        emit("- **unexpected failures: {}**".format(len(unexpected)))
        for name in unexpected:
            emit("::error::Unexpected test failure: {}".format(name))
        return 1

    emit("- no unexpected failures")
    return 0


if __name__ == "__main__":
    sys.exit(main())
