#!/usr/bin/env python3
# scripts/format_code.py
"""Format project C++/Objective-C++ files; leave dependencies and builds alone."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Report formatting differences without editing")
    parser.add_argument("--clang-format", default="clang-format", help="Formatter executable or path")
    args = parser.parse_args()
    formatter = shutil.which(args.clang_format)
    if not formatter:
        parser.error("clang-format is optional for development; install it or pass --clang-format /path/to/clang-format")
    root = Path(__file__).resolve().parent.parent
    files = sorted(
        path
        for directory in (root / "src", root / "tests")
        for path in directory.rglob("*")
        if path.suffix in (".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx", ".mm")
    )
    flags = ["--dry-run", "--Werror"] if args.check else ["-i"]
    result = subprocess.run([formatter, *flags, *map(str, files)], cwd=root)
    if not result.returncode:
        print(f"{'Checked' if args.check else 'Formatted'} {len(files)} project files.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
