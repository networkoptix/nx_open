#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""
Since cmake language lacks some important functions (e.g. the possibility to declare a custom
target transitively dependent on the dependencies of its dependencies), these functions can be
implemented by changing the ninja build files, generated by cmake. The classes defined in this file
(and their subclasses) are intended for parsing and patching these files.

NinjaFileProcessor: The base class for NinjaFileProcessor and RulesNinjaFileProcessor.
NinjaFileProcessorError: The base class for NinjaFileProcessor-related exceptions.
NinjaFileProcessorIOError: Exception of a file IO error.
NinjaFileProcessorParseError: Exception of a file parsing error.

Line: Represents a line of build.ninja file.
LineType: Enumeration to distinguish different types of Line objects.
"""

import os
import re
from collections import namedtuple
from enum import Enum
from pathlib import Path
from typing import TextIO
from abc import ABCMeta, abstractmethod


Line = namedtuple('Line', 'raw parsed type')


class LineType(Enum):
    UNKNOWN = 0
    BUILD = 1
    COMMAND = 2
    DEPFILE = 3
    INCLUDE = 4
    RULE = 5


class NinjaFileProcessor(metaclass=ABCMeta):
    """The base class for processing ninja files.

    :raises NinjaFileProcessorIOError: File can't be read/written/stat-queried.
    :raises NinjaFileProcessorParseError: Wrong file format (file can't be parsed).
    """

    _PATCH_MARKER = "# File is patched by ninja_tool\n"

    # Escape with the "$" char the following chars: newline, space, `:`, `$`.
    _ESCAPE_RE = re.compile(r"([\n \:\$])")

    # Remove `$` char before the following chars: newline, space, `:`, `$`.
    _UNESCAPE_RE = re.compile(r"\$([\n \:\$])")

    _ADDED_LINES_MARKER_START = "# Start of lines added by ninja_tool\n"

    def __init__(self, file_name: Path, build_directory: Path, debug_output: bool = False) -> None:
        self.build_directory = Path(build_directory)
        self._lines = []
        self._added_lines = []
        self._file_name = Path(file_name)
        self._is_patch_applied = False
        self._debug_output = debug_output
        self._current_parsed_line = 0

    def needs_patching(self, script_version_timestamp: float = None) -> bool:
        """Check if build.ninja file needs patching.

        :param script_version_timestamp: Unix timestamp of the patch script file.
        :type script_version_timestamp: float
        :return: Whether the file is not patched yet.
        :rtype: bool
        """

        try:
            # If the currently processed file is older than the patch script, patch it.
            if script_version_timestamp is not None:
                if self._file_name.stat().st_mtime < script_version_timestamp:
                    return True

            with open(self._file_name) as file:
                first_line = next(file)
        except OSError as ex:
            raise NinjaFileProcessorIOError(self, "check patch status of") from ex

        return first_line != self._PATCH_MARKER

    def load_data(self) -> None:
        self._current_parsed_line = 0
        try:
            with open(self._file_name) as ninja_file:
                self._load_data_from_file(ninja_file)
        except OSError as ex:
            raise NinjaFileProcessorIOError(self, "load") from ex

    def _load_data_from_file(self, file: TextIO) -> None:
        for line in file:
            if line == self._ADDED_LINES_MARKER_START:
                break

            self._current_parsed_line += 1

            # "$" followed by a newline escapes the newline.
            while line.endswith("$\n"):
                line += next(file, None)
                if line is None:
                    raise NinjaFileProcessorParseError(self,
                        "another line expected after \"$\" in the end of the line")
                self._current_parsed_line += 1

            line_data = self._parse_line(line)
            self._consume_line_object(line_data)
            self._lines.append(line_data)

    @abstractmethod
    def _parse_line(self, line: str) -> Line:
        pass

    @abstractmethod
    def _consume_line_object(self, line_object: Line) -> None:
        pass

    def save_data(self) -> None:
        assert self.is_loaded(), "Nothing to save (file wasn't loaded?)"

        if not self._is_patch_applied:
            # Do nothing if we haven't patched the file yet.
            return

        patched_file_name = f"{self._file_name}.patched"
        try:
            with open(patched_file_name, "w") as file:
                # Add a patch marker if we applied the patch and the marker isn't already here.
                if not self._has_patch_marker():
                    file.write(self._PATCH_MARKER)

                for record in self._lines:
                    file.write(record.raw)

                if self._added_lines:
                    if self._lines[-1].raw != "\n":
                        file.write("\n")
                    file.write(self._ADDED_LINES_MARKER_START)
                    for record in self._added_lines:
                        file.write(record.raw)
        except OSError as ex:
            raise NinjaFileProcessorIOError(self, "save") from ex

        os.replace(patched_file_name, self._file_name)

        print(f"{self._file_name} patched.")

    def _has_patch_marker(self):
        return self._lines and self._lines[0].raw == self._PATCH_MARKER

    @classmethod
    def _escape_set(cls, unescaped_strings: set) -> set:
        return set(cls._escape_string(string) for string in unescaped_strings)

    @classmethod
    def _escape_string(cls, unescaped_string: str) -> str:
        return re.sub(cls._ESCAPE_RE, "$\\1", unescaped_string)

    @classmethod
    def _unescape_set(cls, escaped_strings: set) -> set:
        return set(cls._unescape_string(string) for string in escaped_strings)

    @classmethod
    def _unescape_string(cls, escaped_string: str) -> str:
        return re.sub(cls._UNESCAPE_RE, "\\1", escaped_string)

    def is_loaded(self) -> bool:
        return len(self._lines) > 0


class NinjaFileProcessorError(Exception):
    pass


class NinjaFileProcessorIOError(NinjaFileProcessorError):
    def __init__(self, file_processor: NinjaFileProcessor, operation: str) -> None:
        error_message = f"Cannot {operation} file {file_processor._file_name}"
        super().__init__(error_message)


class NinjaFileProcessorParseError(NinjaFileProcessorError):
    def __init__(self, file_processor: NinjaFileProcessor, message: str) -> None:
        error_message = (
            f"Cannot parse file {file_processor._file_name}. "
            f"Error at line {file_processor._current_parsed_line}: {message}")
        super().__init__(error_message)
