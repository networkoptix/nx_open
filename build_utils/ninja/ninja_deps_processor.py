#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import subprocess
from pathlib import Path
from typing import List


class NinjaDepsProcessor:
    NINJA_CALL_TIMEOUT_S = 120

    def __init__(self, build_dir: Path):
        self._build_dir = build_dir
        self._build_dir_str = build_dir.as_posix()
        self._outputs_by_dependencies = {}

    def load_data(self):
        deps_data = subprocess.run(
            ["ninja", "-C", str(self._build_dir), "-t", "deps"],
            capture_output=True,
            timeout=self.NINJA_CALL_TIMEOUT_S)

        deps_data.check_returncode()
        self._parse_ninja_deps_output(deps_data.stdout.decode("utf-8"))

    def get_dependent_object_files(self, file_path: Path) -> List[str]:
        return self._outputs_by_dependencies.get(file_path.as_posix(), [])

    def _parse_ninja_deps_output(self, deps_data: str):
        self._outputs_by_dependencies = {}

        current_target = None
        line_number = 0
        for line in deps_data.splitlines():
            line_number += 1
            if not line.startswith(" "):
                new_current_target = line[0:line.find(": ")]
                if new_current_target:
                    # In build.ninja targets are written in the native format (e.g. with "\" as a
                    # directory separator on Windows), so we have to use the same format
                    # everywhere.
                    current_target = str(Path(new_current_target))
                continue

            if current_target is None:
                raise RuntimeError(
                    f"Unexpected output from \"ninja -t deps\" command in line {line_number}")

            file_path_string = self._generate_absolute_file_path_string(line)

            self._outputs_by_dependencies.setdefault(file_path_string, []).append(current_target)

    def _generate_absolute_file_path_string(self, line: str) -> str:
        file_name = line.lstrip()
        if file_name[0:3] != "../":
            return file_name

        file_path = f"{self._build_dir_str}/{file_name}"
        start, end = None, None
        i = 0
        while i < len(file_path):
            if file_path[i:i + 3] == "../":
                if start is None:
                    start = file_path.rfind("/", 0, i - 1)
                else:
                    start = file_path.rfind("/", 0, start)
                i += 3
                end = i
                continue

            if start is not None:
                break
            i += 1

        if start is None or end is None:
            return file_path

        return f"{file_path[0:start]}/{file_path[end:]}"
