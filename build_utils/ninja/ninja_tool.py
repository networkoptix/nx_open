#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""A tool for patching ninja rules/build files generated by CMake."""

from __future__ import annotations
import os
import sys
import argparse
import traceback
import shlex
import subprocess
from typing import List, NamedTuple, Optional, Set
from pathlib import Path

from ninja_deps_processor import NinjaDepsProcessor
from ninja_file_processor.ninja_file_processor import NinjaFileProcessorError
from ninja_file_processor.build_ninja_processor import BuildNinjaFileProcessor
from ninja_file_processor.rules_ninja_processor import RulesNinjaFileProcessor

NINJA_BUILD_FILE_NAME = 'build.ninja'
NINJA_PREBUILD_FILE_NAME = 'pre_build.ninja_tool'
KNOWN_FILES_FILE_NAME = "known_files.txt"
PERSISTENT_KNOWN_FILES_FILE_NAME = "persistent_known_files.txt"
ALLOWED_COMMANDS = [
    "strengthen",
    "run",
    "clean",
    "list_unknown_files",
    "generate_affected_targets_list",
    "add_directories_to_known_files",
]

class ParsedScriptData(NamedTuple):
    strengthened_targets: Set[str] = set()
    commands_to_run: List[List[str]] = list()
    do_clean: bool = None
    do_list_unknown: bool = None
    known_file_names: Set[str] = set()
    changed_files_list_file_name: str = None
    affected_targets_list_file_name: str = None
    source_dir: Path = None
    added_known_directories: Set[str] = set()

    @classmethod
    def merge(cls, sources: List[ParsedScriptData]) -> ParsedScriptData:
        def _last_value_or_none(attr: str):
            return (
                [getattr(s, attr) for s in sources if getattr(s, attr) is not None] or
                [None])[-1]

        if not sources:
            return ParsedScriptData()

        return ParsedScriptData(
            strengthened_targets={t for s in sources for t in s.strengthened_targets},
            commands_to_run=[c for s in sources for c in s.commands_to_run],
            do_clean=any([s for s in sources if s.do_clean]),
            do_list_unknown=any([s for s in sources if s.do_list_unknown]),
            known_file_names={n for s in sources for n in s.known_file_names},
            changed_files_list_file_name=_last_value_or_none("changed_files_list_file_name"),
            affected_targets_list_file_name=_last_value_or_none("affected_targets_list_file_name"),
            source_dir=_last_value_or_none("source_dir"),
            added_known_directories={d for s in sources for d in s.added_known_directories})


def clean_build_directory(build_dir: Path,
        build_file_processor: BuildNinjaFileProcessor = None,
        remove_unknown_files: bool = True,
        additional_known_files: Set[str] = None) -> None:

    if build_file_processor is None:
        build_filename = build_dir / NINJA_BUILD_FILE_NAME
        build_file_processor = BuildNinjaFileProcessor(build_filename, build_directory=build_dir)

    if not build_file_processor.is_loaded():
        build_file_processor.load_data()

    build_files = build_file_processor.get_known_files()
    known_files = get_files_from_list_file(build_dir / KNOWN_FILES_FILE_NAME)
    persistent_known_files = get_files_from_list_file(build_dir / PERSISTENT_KNOWN_FILES_FILE_NAME)
    if (build_dir / "conan_imported_files.txt").exists():
        conan_files = get_files_from_conan_manifest(build_dir / "conan_imported_files.txt")
    else:
        conan_files = get_files_from_conan_manifest(build_dir / "conan_imports_manifest.txt")

    all_known_files = additional_known_files if additional_known_files else set()
    for file_name in build_files | known_files | persistent_known_files | conan_files:
        file_path = Path(file_name)
        if file_path.is_absolute():
            try:
                file_path_relative = file_path.relative_to(build_dir)
                all_known_files.add(file_path_relative.as_posix())
            except ValueError:
                pass
        else:
            all_known_files.add(file_path.as_posix())

    extra_files = find_extra_files(build_dir, all_known_files)

    if remove_unknown_files:
        print("Cleaning build directory...")

    for file in extra_files:
        if not os.path.isfile(file) and not os.path.islink(file):
            continue
        if remove_unknown_files:
            os.remove(file)
        else:
            print(file)

    if remove_unknown_files:
        print("Done")


def get_files_from_list_file(list_file_name: Path) -> set:
    if not list_file_name.is_file():
        return set()

    result = set()
    with open(list_file_name) as list_file:
        for line in list_file:
            entry = line.strip()
            path = Path(entry)

            # If an entry ends with a slash, consider it as a directory which contents should be
            # ignored. All files will be added to the list of known files.
            if entry.endswith("/") and path.exists():
                for root, _, files in os.walk(path):
                    root = os.path.normpath(root)
                    for file in files:
                        result.add(str(Path(root) / file))

            result.add(str(path))

    return result


def get_files_from_conan_manifest(conan_manifest_file_name: Path) -> set:
    result = set()
    try:
        with open(conan_manifest_file_name) as manifest:
            for line in manifest:
                sep = line.find(': ')
                if sep > 0:
                    file_name = line[:sep]
                    result.add(str(Path(file_name).as_posix()))
    except OSError:
        pass

    return result


def find_extra_files(build_dir: Path, known_files: set) -> list:
    """Find files not needed to the build."""

    exclusion_dirs = {
        'CMakeFiles',
        '_autogen',
        ".conan",
        ".conan_short",
        ".cmake",
    }

    exclusion_extensions = {
        '.pdb',
        '.cpp_parameters',
        '.cab',
        '.CABinet',
    }

    exclusions = {
        NINJA_BUILD_FILE_NAME,
        NINJA_PREBUILD_FILE_NAME,
        PERSISTENT_KNOWN_FILES_FILE_NAME,
        "compile_commands.json",
        "CTestTestfile.cmake",
        "cmake_install.cmake",
        "CMakeCache.txt",
        "rules.ninja",
        ".ninja_deps",
        ".ninja_log",
        "conanbuildinfo.cmake",
        "conanbuildinfo.txt",
        "conaninfo.txt",
        "conan.lock",
        "conan_imports_manifest.txt",
        "conan_imported_files.txt",
        "graph_info.json",
        "pre_build.log",
    }

    result = []

    for file in build_dir.rglob('*'):
        if any(d in file.parent.parts for d in exclusion_dirs):
            continue
        if file.name in exclusions or file.suffix in exclusion_extensions:
            continue
        if str(file.relative_to(build_dir).as_posix()) in known_files:
            continue

        result.append(build_dir / file)

    return result


def execute_command(build_dir: Path, command_with_args: List[str], force_patch: bool) -> None:
    script_data = _parse_splitted_command_line(command_with_args[0], command_with_args[1:])
    _execute(build_dir, script_data, force_patch)


def execute_script(build_dir: Path, script_file_name: str, force_patch: bool) -> None:
    script_data = _parse_script_data(script_file_name)
    _execute(build_dir, script_data, force_patch)


def _execute(
        build_dir: Path,
        script_data: ParsedScriptData,
        force_patch: bool,
        script_file_name: str = None):
    build_file_name = build_dir / NINJA_BUILD_FILE_NAME
    build_file_processor = BuildNinjaFileProcessor(build_file_name, build_directory=build_dir)

    if script_data.changed_files_list_file_name:
        generate_list_of_targets_affected_by_listed_files(
            build_dir=build_dir, source_dir=script_data.source_dir,
            changed_files_list_file_name=script_data.changed_files_list_file_name,
            affected_targets_list_file_name=script_data.affected_targets_list_file_name,
            build_file_processor=build_file_processor)

    if script_data.added_known_directories:
        update_persistent_known_files_file(
             build_dir=build_dir, directories=script_data.added_known_directories)

    if script_data.do_list_unknown:
        clean_build_directory(build_dir=build_dir, build_file_processor=build_file_processor,
            additional_known_files=script_data.known_file_names, remove_unknown_files=False)

    if script_data.do_clean:
        clean_build_directory(build_dir=build_dir, build_file_processor=build_file_processor,
            additional_known_files=script_data.known_file_names)

    if _has_data_for_patching(script_data):
        if script_file_name is not None:
            script_timestamp = os.path.getmtime(script_file_name)
        else:
            script_timestamp = None
        patch_ninja_build(file_name=build_file_name,
            strengthened_targets=script_data.strengthened_targets,
            script_timestamp=script_timestamp,
            build_file_processor=build_file_processor,
            force_patch=force_patch)

    for command in script_data.commands_to_run:
        print(f"Running {command}...")
        subprocess.call(command)

    print("All done")


def update_persistent_known_files_file(build_dir: Path, directories: Set[str]):
    print(f"Updating persistent known files file...")

    persistent_known_files = build_dir / PERSISTENT_KNOWN_FILES_FILE_NAME
    if persistent_known_files.is_file():
        with open(persistent_known_files) as f:
            current_files = [Path(name) for name in f.read().splitlines()]
    else:
        current_files = []

    unchanged_files = []
    for file_path in current_files:
        if set([p for p in file_path.parents] + [file_path]).intersection(directories):
            continue
        unchanged_files.append(file_path)

    print(f"Adding files from directories {[str(d) for d in directories]!r}...")
    new_files = [f for d in directories for f in d.rglob("*")]
    with open(persistent_known_files, "w") as f:
        f.writelines({f"{str(f)}\n" for f in unchanged_files + new_files})

    print("Done")


def _has_data_for_patching(script_data: ParsedScriptData):
    return bool(script_data.strengthened_targets)


def _parse_script_data(script_file_name: str) -> ParsedScriptData:
    parsed_data_list = []
    print(f"Reading script {script_file_name}...")

    with open(script_file_name) as script_file:
        for line in script_file:
            line = line.rstrip()
            if not line:
                continue
            command, *args = shlex.split(line)
            parsed_data_list.append(_parse_splitted_command_line(command, args))

    print("Done")
    return ParsedScriptData.merge(parsed_data_list)


def _parse_splitted_command_line(command: str, args: List[str]) -> ParsedScriptData:
    if command == "strengthen":
        if not args:
            print("Missing argument for \"strengthen\" command.")
            return ParsedScriptData()
        return ParsedScriptData(strengthened_targets={args[0]})

    if command == "run":
        if not args:
            print("Missing argument for \"run\" command.")
            return ParsedScriptData()
        return ParsedScriptData(commands_to_run=[args.copy()])

    if command == "clean":
        return ParsedScriptData(do_clean=True)

    if command == "list_unknown_files":
        return ParsedScriptData(do_list_unknown=True)

    if command == "generate_affected_targets_list":
        if len(args) != 3:
            print("There must be exactly three arguments for "
                '"generate_affected_targets_list" command.')
            return ParsedScriptData()
        return ParsedScriptData(
            source_dir=Path(args[0]),
            changed_files_list_file_name=args[1],
            affected_targets_list_file_name=args[2],
            known_file_names={args[1], args[2]})

    if command == "add_directories_to_known_files":
        if not len(args):
            print("There must be at least one argument for "
                '"add_directories_to_known_files" command.')
            return ParsedScriptData()
        return ParsedScriptData(
            added_known_directories=set([Path(d).absolute() for d in args]))

    print(f"Unknown command: {command}")
    return ParsedScriptData()


def generate_list_of_targets_affected_by_listed_files(
        build_dir: Path, source_dir: Path,
        changed_files_list_file_name: str,
        affected_targets_list_file_name: str,
        build_file_processor: BuildNinjaFileProcessor = None):
    print(f"Generating list of affected targets...")

    updated_targets = set()
    with open(build_dir / changed_files_list_file_name) as f:
        files = f.read().splitlines()

    build_file_processor.load_data()
    ninja_deps_processor = NinjaDepsProcessor(build_dir)
    ninja_deps_processor.load_data()
    for file in files:
        full_path = source_dir / file
        # Get targets that explicitly depend on the changed file.
        updated_targets |= build_file_processor.get_changed_targets_by_file_name(full_path)
        # Get targets (object files) that implicitly depend on the changed file. For each such
        # target, get all the targets that depend on it.
        for target_file in ninja_deps_processor.get_dependent_object_files(full_path):
            updated_targets |= build_file_processor.get_changed_targets_by_file_name(target_file)

    try:
        with open(build_dir / affected_targets_list_file_name) as f:
            old_targets = set([l.rstrip() for l in f.readlines()])
    except:
        old_targets = None

    # Prevent overwriting of the output file (and hence rebuilding dependent targets) if its
    # content hasn't changed.
    if old_targets != updated_targets:
        with open(build_dir / affected_targets_list_file_name, "w") as f:
            f.write("\n".join(updated_targets))

    print("Done")


def patch_ninja_build(
        file_name: Path,
        strengthened_targets: set,
        build_file_processor: BuildNinjaFileProcessor,
        script_timestamp: float = None,
        force_patch: bool = False) -> None:
    """Do patching of build.ninja. Also searches for the "include" directive for rules.ninja, and
    patches rules.ninja too.

    :param file_name: "build.ninja" file name, including the full path to it.
    :type file_name: str
    :param strengthened_targets: Set of targets to make transitively dependent on their
        dependencies.
    :type strengthened_targets: set
    :param script_timestamp: Modification time of the script file.
    :type script_timestamp: float
    :param build_file_processor: The instance of BuildNinjaFileProcessor class.
    :type build_file_processor: BuildNinjaFileProcessor
    :param force_patch: Whether to force-patch the file, regardless of the script modification time,
        defaults to False
    :type force_patch: bool, optional
    """

    # Patch the file only if it needs patching (it was not patched yet or its modification time is
    # older than the modification time of the patch script) or the patching is enforced by the
    # command-line parameter.
    if not force_patch:
        if not build_file_processor.needs_patching(script_version_timestamp=script_timestamp):
            print(f"{file_name} is already patched, do nothing")
            return

    if not build_file_processor.is_loaded():
        build_file_processor.load_data()

    print("Patching build.ninja...")
    patch_rules_file(
        build_file_processor, script_timestamp=script_timestamp, force_patch=force_patch)
    if strengthened_targets:
        build_file_processor.strengthen_dependencies(strengthened_targets)
    build_file_processor.save_data()
    print("Done")


def patch_rules_file(
        build_file_processor: BuildNinjaFileProcessor,
        script_timestamp: float = None,
        force_patch: bool = False) -> None:

    rules_file_name = build_file_processor.get_rules_file_name()
    rules_file_processor = RulesNinjaFileProcessor(
        rules_file_name, build_directory=build_file_processor.build_directory)

    if not force_patch:
        if not rules_file_processor.needs_patching(script_version_timestamp=script_timestamp):
            print(f"{rules_file_name} is already patched, do nothing")
            return

    rules_file_processor.load_data()

    print("Patching rules.ninja...")
    rules_file_processor.patch_cmake_rerun()
    rules_file_processor.save_data()
    print("Done")


def redirect_output(file_name: Path):
    """Redirects stderr and stdout to the file."""

    log_dir = file_name.parent
    log_dir.mkdir(parents=True, exist_ok=True)

    log_fd = open(file_name, 'w')
    sys.stdout = log_fd
    sys.stderr = log_fd


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-d", "--build-dir",
        type=Path,
        help="Ninja build directory.",
        default=Path.cwd())
    parser.add_argument(
        "-f", "--force",
        action='store_true',
        help="Force ninja file patching.")
    parser.add_argument(
        "-o", "--log-output",
        nargs="?",
        type=str,
        default=None,
        const="",
        help='Log output to file.')
    parser.add_argument(
        "-t", "--stack-trace",
        action='store_true',
        help="Print stack trace if an error occurred.")
    parser.add_argument(
        "-e", "--execute",
        nargs="+",
        dest="command",
        help=(
            "Execute specified command and exit. First value for this parameter is the command "
            f"and must be one of {', '.join([repr(c) for c in ALLOWED_COMMANDS])}. Other values "
            "are command parameters, theirs number and meaning depend on the command."))

    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    if args.log_output is not None:
        log_file = Path(args.log_output or (build_dir / "build_logs" / "pre_build.log"))
        redirect_output(log_file)

    try:
        if args.command:
            execute_command(
                build_dir=build_dir, command_with_args=args.command, force_patch=args.force)
        else:
            script_filename = build_dir / NINJA_PREBUILD_FILE_NAME
            execute_script(
                build_dir=build_dir, script_file_name=script_filename, force_patch=args.force)

    except NinjaFileProcessorError as ex:
        if args.stack_trace:
            message = ''.join(traceback.TracebackException.from_exception(ex).format())
        else:
            message = str(ex)
        sys.exit(f"{message}\nAn error occured while processing ninja files, exiting")

    except OSError as ex:
        if args.stack_trace:
            message = ''.join(traceback.TracebackException.from_exception(ex).format())
        else:
            message = str(ex)
        sys.exit(f"{message}\nAn error occured while performing file operation, exiting")

if __name__ == "__main__":
    main()
