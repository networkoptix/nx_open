#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from pathlib import Path
import subprocess
import sys


def main():
    command = ['pip', 'show', '--files', 'cmake']
    result = subprocess.run(command, capture_output=True)
    if result.returncode != 0:
        output = result.stderr.decode('ascii')
        print(f'ERROR: Command "{" ".join(command)}" failed: {output!r}.', file=sys.stderr)
        sys.exit(result.returncode)

    output = result.stdout.decode('ascii')

    package_location: Path = None
    cmake_executable_path: Path = None

    for line in output.splitlines():
        if line.startswith("Location:"):
            package_location = Path((line.partition(':')[2].strip()))
            continue

        if line.endswith('cmake.exe'):
            cmake_executable_path = Path(line.strip())

    if package_location is None or cmake_executable_path is None:
        print(
            f'ERROR: Cannot parse output of "{" ".join(command)}" command.',
            file=sys.stderr)
        sys.exit(1)

    print((package_location / cmake_executable_path).as_posix())


if __name__ == '__main__':
    main()
