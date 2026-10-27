#!/usr/bin/env python3
"""Generate clangd commands from the kernel, user, and host-tool Makefiles."""

import json
import shlex
import subprocess
from pathlib import Path


OS = Path(__file__).resolve().parents[1]
DATABASE = OS / "compile_commands.json"


def dry_run(directory: Path, *targets: str):
    output = subprocess.check_output(
        ["make", "-B", "-n", "-s", *targets], cwd=directory, text=True
    )
    for line in output.splitlines():
        words = shlex.split(line)
        if words and words[0].endswith(("gcc", "cc")):
            yield words


def compile_args(command: list[str], source: str) -> list[str]:
    """Turn a link command with several C files into one compile command."""
    args = []
    skip_next = False
    for word in command:
        if skip_next:
            skip_next = False
        elif word in ("-o", "-T"):
            skip_next = True
        elif word == "-c" or word.endswith(".c") or word.startswith("-Wl,"):
            continue
        else:
            args.append(word)
    return [*args, "-c", source]


def add_commands(entries: dict[str, dict], directory: Path, commands, *, replace=False):
    for command in commands:
        sources = [word for word in command if word.endswith(".c")]
        for source in sources:
            path = (directory / source).resolve()
            if not path.is_file():
                continue
            key = str(path)
            if key not in entries or replace:
                entries[key] = {
                    "directory": str(directory),
                    "file": key,
                    "arguments": compile_args(command, source),
                }


def main():
    entries = {}
    add_commands(entries, OS / "user", dry_run(OS / "user", "all"))
    add_commands(entries, OS, dry_run(OS, "os.elf"), replace=True)
    add_commands(entries, OS, dry_run(OS, "mkfs.out", "test_easyfs.out"))
    DATABASE.write_text(json.dumps([entries[k] for k in sorted(entries)], indent=2) + "\n")
    print(f"{DATABASE}: {len(entries)} C translation units")


if __name__ == "__main__":
    main()
