#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import json
import os
from pathlib import Path
import sys


def substitute_msvc_macros(path, file):
    '''
    Substitute MSVC macros in the provided path.
    The following macros can be used in CMakeSettings.json:
        - ${workspaceRoot} - the full path of the workspace folder
        - ${workspaceHash} - hash of workspace location; useful for creating a unique identifier
            for the current workspace (for example, to use in folder paths)
        - ${projectFile} - the full path of the root CMakeLists.txt file
        - ${projectDir} - the full path of the folder containing the root CMakeLists.txt file
        - ${projectDirName} - the name of the folder containing the root CMakeLists.txt file
        - ${thisFile} - the full path of the CMakeSettings.json file
        - ${name} - the name of the configuration
        - ${generator} - the name of the CMake generator used in this configuration
    We support only ${projectDir} macro as it is used in the our template files.
    '''
    PROJECT_DIR_MACRO = '${projectDir}'
    if PROJECT_DIR_MACRO in path:
        path = path.replace(PROJECT_DIR_MACRO, Path(file).parent.absolute().as_posix())
    return path


def substitute_env_variables(path):
    '''
    Substitute environment variables in the provided path. Visual Studio format is supported:
    ${env.VARIABLE_NAME}
    '''
    START_TOKEN = '${env.'
    END_TOKEN = '}'
    while START_TOKEN in path and END_TOKEN in path:
        start_index = path.index(START_TOKEN)
        end_index = path.index(END_TOKEN, start_index)
        var = path[start_index + len(START_TOKEN):end_index]
        path = path.replace(START_TOKEN + var + END_TOKEN, os.environ.get(var))

    return path


def replace_conan_qt_path(file, qtdir, build_root):
    print(f"Substitute actual Qt path to the {file})")
    if not os.path.exists(file):
        print(f"File {file} does not exists")
        return 1

    modified = False
    binaries_dir = (Path(qtdir) / 'bin').as_posix()
    with open(file, 'r', encoding='utf-8-sig') as source:
        json_root = json.load(source)
        for json_configuration in json_root['configurations']:
            configuration_build_root = Path(substitute_env_variables(substitute_msvc_macros(
                json_configuration['buildRoot'], file)))
            if configuration_build_root != build_root:
                print(
                    f"Skip configuration {json_configuration['name']} as it's build root "
                    + f"{json_configuration['buildRoot']} does not match {build_root}")
                continue

            if 'environments' not in json_configuration:
                modified = True
                json_configuration['environments'] = [{'qtdir': binaries_dir}]
            else:
                json_environments = json_configuration['environments']
                for json_environment in json_environments:
                    if 'qtdir' in json_environment and json_environment['qtdir'] == binaries_dir:
                        print(f"Found existing path {json_environment['qtdir']}")
                        continue
                    modified = True
                    json_environment['qtdir'] = binaries_dir

    if modified:
        print(f"Storing changes to {file}")
        with open(file, 'w', encoding='utf-8') as target:
            json.dump(json_root, target, indent=2)
    else:
        print(f"All {len(json_root['configurations'])} configurations contain valid path")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Substitute qt path in the Visual Studio cmake configuration file.")
    parser.add_argument("file", type=str, help="Path to CMakeSettings.json")
    parser.add_argument("--qtdir", type=str, help="Path to qt dir")
    parser.add_argument("--build_root", type=str, help="Build root, which should be processed")
    args = parser.parse_args()
    sys.exit(replace_conan_qt_path(args.file, args.qtdir, Path(args.build_root)))


if __name__ == "__main__":
    main()
