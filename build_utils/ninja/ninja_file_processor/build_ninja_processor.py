#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""
BuildNinjaFileProcessor: The class for processing ninja file, containing targets ("build.ninja").
"""

import re
from collections import namedtuple
from dataclasses import dataclass
from pathlib import Path
from typing import Tuple, Set

from .ninja_file_processor import (
    NinjaFileProcessor,
    NinjaFileProcessorParseError,
    Line,
    LineType)


CommandLineData = namedtuple('CommandLineData', 'command')
DepfileLineData = namedtuple('DepfileLineData', 'depfile')

@dataclass
class BuildLineData:
    outputs: list
    implicit_outputs: list
    command: str
    dependencies: list
    implicit_dependencies: list
    order_only_dependencies: list

    def all_dependencies(self) -> set:
        return (set(self.dependencies) | set(self.implicit_dependencies) |
                set(self.order_only_dependencies))


class BuildNinjaFileProcessor(NinjaFileProcessor):
    BuildResults = namedtuple('BuildReulsts', 'outputs, implicit_outputs')
    BuildSources = namedtuple(
        'BuildSources', 'command dependencies implicit_dependencies order_only_dependencies')

    # Trailing "$" symbols.
    _ESCAPE_SYMBOLS_RE = re.compile(r".+(\$+)$")

    # Universal regex for parsing specific build.ninja syntax.
    _LINE_RE = re.compile(r"\s+(?P<parameter>COMMAND|depfile)\s*=\s*(?P<value>.+)"
        r"|include\s+(?P<rules_file_name>.*rules.ninja)\s*"
        r"|build\s+(?P<build_line>.+)")

    _MOCARG_RE = re.compile(r'.*moc(?:.exe)? @([^"]+)')

    def __init__(self, file_name: Path, build_directory: Path, debug_output: bool = False) -> None:
        self._line_number_by_output = {}
        self._outputs_by_dependencies = {}
        self._rules_file_name = Path("")
        super().__init__(
            file_name=file_name, build_directory=build_directory, debug_output=debug_output)

    def _parse_line(self, line: str) -> Line:  # pylint:disable=inconsistent-return-statements
        match = self._LINE_RE.match(line)

        if match is None:
            return Line(raw=line, parsed=None, type=LineType.UNKNOWN)

        if match['build_line']:
            parsed_build_data = self._parse_build_line(match['build_line'])
            return Line(
                    raw=line,
                    parsed=parsed_build_data,
                    type=LineType.BUILD)

        if match['parameter'] == 'COMMAND':
            return Line(raw=line, parsed=match['value'], type=LineType.COMMAND)

        if match['parameter'] == 'depfile':
            return Line(raw=line, parsed=match['value'], type=LineType.DEPFILE)

        if match['rules_file_name']:
            return Line(raw=line, parsed=match['rules_file_name'], type=LineType.INCLUDE)

        assert False, "Parsing regex error for line {line!r}"

    def _consume_line_object(self, line_object: Line) -> None:
        if line_object.type == LineType.BUILD:
            line_data = line_object.parsed
            # Add explicit and implicit outputs to output to line number map.
            current_outputs = {*line_data.outputs, *line_data.implicit_outputs}
            for output in current_outputs:
                self._line_number_by_output[output] = len(self._lines)
            for dependency in line_data.all_dependencies():
                self._outputs_by_dependencies.setdefault(dependency, set()).update(current_outputs)

        elif line_object.type == LineType.INCLUDE:
            self._rules_file_name = Path(line_object.parsed)

    def _parse_build_line(self, build_line: str) -> BuildLineData:
        try:
            results, sources = self._split_build_line(build_line.rstrip())
        except ValueError:
            raise NinjaFileProcessorParseError(
                self, f"invalid \"build\" line {build_line!r}.") from None

        build_results = self._parse_build_results_string(results)
        build_sources = self._parse_build_sources_string(sources)

        return BuildLineData(
            outputs=build_results.outputs,
            implicit_outputs=build_results.implicit_outputs,
            command=build_sources.command,
            dependencies=build_sources.dependencies,
            implicit_dependencies=build_sources.implicit_dependencies,
            order_only_dependencies=build_sources.order_only_dependencies)

    def _split_build_line(self, line: str) -> Tuple[str, str]:
        """Splits build line into "results" and "sources" parts.

        :param line: Source line.
        :type line: str
        :raises NinjaFileProcessorParseError: Bad build line format.
        :return: "results" and "sources" parts.
        :rtype: Tuple[str, str]
        """

        tokens = line.split(':')
        if len(tokens) < 2:
            raise NinjaFileProcessorParseError(self, f"invalid \"build\" line {line!r}.")

        [results, sources] = (tokens.pop(0), "")

        # This is needed because the ":" symbol (field separator) can be escaped by an odd number
        # of "$" symbols. So we go through all tokens and check - if the previous one ends with the
        # odd number of escaping symbols, then the new one is not a separate token but is a
        # continuation of the previous one (and, by transitivity, is a continuation of the
        # "results" part). Once we find the first separate token, we stop and consider all
        # remaining tokens to be the "sources" part.
        while tokens:
            token = tokens.pop(0)
            if results.endswith('$'):
                match = self._ESCAPE_SYMBOLS_RE.match(results)
                if len(match[1]) % 2 == 1:
                    results += f":{token}"
                    continue
            sources = token + (':' if tokens else '')
            break

        sources += ':'.join(tokens)
        if not sources:
            raise NinjaFileProcessorParseError(self, f"invalid \"build\" line {line!r}.")

        return results, sources.lstrip()

    def _parse_build_results_string(self, results: str) -> BuildResults:  # pylint:disable=undefined-variable
        explicit_raw, has_implicit, implicit_raw = results.partition("|")
        if "|" in implicit_raw:
            raise NinjaFileProcessorParseError(self, f"invalid outputs list {results!r}.")

        explicit_outputs = self._tokenize(explicit_raw)
        implicit_outputs = self._tokenize(implicit_raw) if has_implicit else []

        return self.BuildResults(outputs=explicit_outputs, implicit_outputs=implicit_outputs)

    def _parse_build_sources_string(self, sources: str) -> BuildSources:  # pylint:disable=undefined-variable
        normal_raw, has_order_only, order_only_raw = sources.partition("||")
        explicit_raw, has_implicit, implicit_raw = normal_raw.partition("|")

        # Check the line validity.
        if "|" in implicit_raw:
            raise NinjaFileProcessorParseError(self, f"invalid dependencies list {sources!r}.")

        if "||" in order_only_raw:
            raise NinjaFileProcessorParseError(self,
                f"invalid order only dependencies list {sources!r}.")

        # Tokenize explicit dependencies and check its validity.
        explicit_dependencies = self._tokenize(explicit_raw)
        if not explicit_dependencies:
            raise NinjaFileProcessorParseError(self, "command not found in \"build\" line.")

        # First token in explicit dependencies is actually a command, not dependency.
        command = explicit_dependencies.pop(0)

        # Tokenize other dependencies.
        implicit_dependencies = self._tokenize(implicit_raw) if has_implicit else []
        order_only_dependencies = self._tokenize(order_only_raw) if has_order_only else []

        return self.BuildSources(
            command=command,
            dependencies=explicit_dependencies,
            implicit_dependencies=implicit_dependencies,
            order_only_dependencies=order_only_dependencies)

    @classmethod
    def _tokenize(cls, line: str) -> list:
        tokens = []

        for token in line.split(' '):
            if not token:
                continue

            if not tokens or not tokens[-1].endswith('$'):
                tokens.append(token)
                continue

            # This is needed because the " " symbol (field separator) can be escaped by an odd
            # number of the "$" symbols. So we check the previous token, and if it ends with an odd
            # number of escaping symbols, then the new token is not a separate token but is a
            # continuation of the previous one. Thus we should concatenate these tokens instead of
            # adding a new token to the list.
            match = cls._ESCAPE_SYMBOLS_RE.match(tokens[-1])
            if len(match[1]) % 2 == 1:
                tokens[-1] += f" {token}"

        return tokens

    def strengthen_dependencies(self, targets: set) -> None:
        """Patch build.ninja file so the targets from the "targets" set become immediately
        dependent on all the dependencies of their dependencies (transitively).

        Parameters:
        targets (set): Set of the targets to be patched.
        """

        escaped_targets = self._escape_set(targets)
        for target in escaped_targets:
            transitive_dependencies = self._collect_transitive_dependencies(target)
            self._replace_target(target,
                implicit_dependencies=transitive_dependencies,
                order_only_dependencies=[])

        self._is_patch_applied = True  # pylint:disable=attribute-defined-outside-init

    def _get_target_line_by_name(self, target: str) -> Line:
        target_line_idx = self._line_number_by_output.get(target, None)
        if target_line_idx is None:
            return None

        return self._lines[target_line_idx]

    @classmethod
    def _is_target_phony(cls, target_line: Line) -> bool:
        assert target_line.type == LineType.BUILD, (
            "Error in logic: trying to check if target is phony for the line with the wrong type.")
        return target_line.parsed.command == "phony"

    def _collect_transitive_dependencies(self, target: str) -> set:
        """Recursively gets all the dependencies that are targets themselves."""

        transitive_dependencies = set()
        target_line = self._get_target_line_by_name(target)
        first_target_data = target_line.parsed
        unchecked_targets = list(first_target_data.all_dependencies())

        while unchecked_targets:
            current_target = unchecked_targets.pop()
            if current_target in transitive_dependencies:
                continue

            current_target_line = self._get_target_line_by_name(current_target)
            if current_target_line is None:
                continue

            transitive_dependencies.add(current_target)

            current_target_data = current_target_line.parsed
            unchecked_targets.extend(list(current_target_data.all_dependencies()))

        # Don't need explicit dependencies of the target in the result.
        transitive_dependencies -= set(first_target_data.dependencies)

        # Remove phony targets and non-targets from the dependencies.
        for dep in list(transitive_dependencies):
            target_line = self._get_target_line_by_name(dep)
            if not target_line or self._is_target_phony(target_line):
                transitive_dependencies.remove(dep)

        return transitive_dependencies

    def _replace_target(self,
            target: str,
            dependencies: list = None,
            implicit_dependencies: list = None,
            order_only_dependencies: list = None) -> None:

        target_line = self._get_target_line_by_name(target)
        if target_line is None:
            print(f"Unknown target {target}")
            return

        target_line_data = target_line.parsed

        if dependencies is not None:
            target_line_data.dependencies = dependencies

        if implicit_dependencies is not None:
            target_line_data.implicit_dependencies = implicit_dependencies

        if order_only_dependencies is not None:
            target_line_data.order_only_dependencies = order_only_dependencies

        build_line_string = self._generate_build_line(target_line_data)
        self._set_target_line_by_name(
            target, target_line._replace(raw=build_line_string, parsed=target_line_data))

    def _generate_build_line(self, record: BuildLineData) -> str:
        line = ' '.join(sorted(record.outputs) if self._debug_output else record.outputs)

        if record.implicit_outputs:
            if self._debug_output:
                line += f" | {' '.join(sorted(record.implicit_outputs))}"
            else:
                line += f" | {' '.join(record.implicit_outputs)}"

        line += f": {record.command}"

        if record.dependencies:
            if self._debug_output:
                line += f" {' '.join(sorted(record.dependencies))}"
            else:
                line += f" {' '.join(record.dependencies)}"

        if record.implicit_dependencies:
            if self._debug_output:
                line += f" | {' '.join(sorted(record.implicit_dependencies))}"
            else:
                line += f" | {' '.join(record.implicit_dependencies)}"

        if record.order_only_dependencies:
            if self._debug_output:
                line += f" || {' '.join(sorted(record.order_only_dependencies))}"
            else:
                line += f" || {' '.join(record.order_only_dependencies)}"

        return f"build {line}\n"

    def _set_target_line_by_name(self, target: str, record: BuildLineData) -> None:
        target_line_idx = self._line_number_by_output.get(target, None)
        if target_line_idx is None:
            return None

        self._lines[target_line_idx] = record

    def get_known_files(self) -> set:
        files = set()
        for line in self._lines:
            line_data = line.parsed

            if line.type == LineType.BUILD:
                files.update({
                    *line_data.outputs,
                    *line_data.implicit_outputs,
                    *line_data.dependencies,
                    *line_data.implicit_dependencies,
                    *line_data.order_only_dependencies})

            elif line.type == LineType.COMMAND:
                match = self._MOCARG_RE.fullmatch(line_data)
                if match:
                    files.add(match[1])

            elif line.type == LineType.DEPFILE:
                files.add(line_data)

        return {f.replace('\\', '/') for f in self._unescape_set(files)}

    def get_rules_file_name(self) -> Path:
        if self._rules_file_name:
            dir_path = self._file_name.parent
            return dir_path.joinpath(self._rules_file_name)

        # rules.ninja isn't included in build.ninja, so the rules must be in build.ninja itself.
        return self._file_name

    def get_changed_targets_by_file_name(self, file_name: Path) -> Set[str]:
        dependency = self._escape_string(str(file_name))
        return self._get_changed_targets_by_dependency(dependency)

    def _get_changed_targets_by_dependency(self, dependency: str) -> Set[str]:
        changed_targets = set()
        current_level_targets = {dependency}
        while True:
            dependent_targets = set()
            for target in current_level_targets:
                dependent_targets |= self._outputs_by_dependencies.get(target, set())
            new_dependent_targets = dependent_targets - changed_targets
            if not new_dependent_targets:
                break
            changed_targets |= new_dependent_targets
            current_level_targets = new_dependent_targets

        return changed_targets
