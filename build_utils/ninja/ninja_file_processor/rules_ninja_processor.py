#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""
RulesNinjaFileProcessor: Performs processing of the rules.ninja file (the file containing build
rules).
"""

import re
import os
import sys
import platform
from pathlib import Path
from typing import Tuple

from .ninja_file_processor import (
    Line,
    LineType,
    NinjaFileProcessor,
    NinjaFileProcessorParseError)


class RulesNinjaFileProcessor(NinjaFileProcessor):  # pylint:disable=too-few-public-methods
    class RuleLookupError(Exception):
        pass

    _RERUN_CMAKE_RULE = "RERUN_CMAKE"
    _VERIFY_GLOBS_RULE = "VERIFY_GLOBS"
    _VERIFY_GLOBS_TOOL_NAME = "verify_globs"
    _SELF_RUN_OPTIONS = "--log-output --stack-trace"

    # Universal regex for parsing specific rules.ninja syntax.
    _LINE_RE = re.compile(
        r'(?P<is_rule>rule (?P<rule_name>.+?))\s+'
        r'|(?P<is_command>\s+command\s+=\s+(?P<command>.+))\s*')

    def __init__(self, file_name: Path, build_directory: Path, debug_output: bool = False) -> None:
        self._line_number_by_rule = {}
        super().__init__(
            file_name=file_name, build_directory=build_directory, debug_output=debug_output)

    def _parse_line(self, line: str) -> Line:
        match = self._LINE_RE.match(line)

        if match is None:
            return Line(raw=line, parsed=None, type=LineType.UNKNOWN)

        if match['is_rule']:
            return Line(raw=line, parsed=match['rule_name'], type=LineType.RULE)

        if match['is_command']:
            return Line(raw=line, parsed=match['command'], type=LineType.COMMAND)

        assert False, "Parsing regex error for line {line!r}"

    def _consume_line_object(self, line_object: Line) -> None:
        if line_object.type == LineType.RULE:
            self._line_number_by_rule[line_object.parsed] = len(self._lines)

    def patch_cmake_rerun(self) -> None:
        """Adds ninja_tool call on CMake regeneration."""

        try:
            command_line_index, command = self._find_command_by_rule(
                self._RERUN_CMAKE_RULE)
        except self.RuleLookupError:
            print(f"Can't find {self._RERUN_CMAKE_RULE} in rules.ninja file.")
            return

        self_run_string = (
            f'"{sys.executable}" "{os.path.abspath(sys.argv[0])}" {self._SELF_RUN_OPTIONS}')
        if self_run_string in command:  # ninja_tool is already added.
            return

        # Add ninja_tool to command line. On Windows we should wrap command in the
        # 'cmd.exe /C "..."' if we want to run more than one command.
        if sys.platform == "win32" and not command.startswith("cmd.exe"):
            updated_rerun_command = f'cmd.exe /C "{command} && {self_run_string}"'
        else:
            updated_rerun_command = command + f" && {self_run_string}"

        updated_line = f"  command = {updated_rerun_command}\n"
        self._lines[command_line_index] = Line(
            raw=updated_line, parsed=None, type=LineType.UNKNOWN)  # pylint:disable=undefined-loop-variable

        self._is_patch_applied = True  # pylint:disable=attribute-defined-outside-init

    def _find_command_by_rule(self, rule: str) -> Tuple[int, str]:
        if rule not in self._line_number_by_rule:
            raise self.RuleLookupError(f"Command {rule} is not found")

        cmake_rule_line_index = self._line_number_by_rule[rule]
        for i in range(cmake_rule_line_index+1, len(self._lines)):
            line_object = self._lines[i]
            if line_object.type == LineType.RULE:
                break

            if line_object.type == LineType.COMMAND:
                return i, line_object.parsed

        raise NinjaFileProcessorParseError(
            self, f'No command found for rule "{rule}"')

    def patch_verify_globs(self, verify_globs_path: Path) -> None:
        """Use custom script to verify globs."""

        try:
            command_line_index, command = self._find_command_by_rule(
                self._VERIFY_GLOBS_RULE)
        except self.RuleLookupError:
            print(f"Can't find {self._VERIFY_GLOBS_RULE} in rules.ninja file.")
            return

        verify_globs_full_path = verify_globs_path.joinpath(self._VERIFY_GLOBS_TOOL_NAME)
        verify_globs_run_string = (
            f"{verify_globs_full_path.as_posix()} {self.build_directory.as_posix()}")
        if verify_globs_run_string == command:  # verify_globs is already added.
            return

        updated_line = f"  command = {verify_globs_run_string}\n"
        self._lines[command_line_index] = Line(
            raw=updated_line, parsed=None, type=LineType.COMMAND)

        self._is_patch_applied = True  # pylint:disable=attribute-defined-outside-init
