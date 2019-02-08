#!/usr/bin/env python

import sys
import argparse
import os
from cmake_config import read_cmake_config
from semantic_output import *
from sources_parser import parse_sources_cached, clear_sources_cache


customizable_projects = [
    'client-dmg',
    'icons',
    'mediaserver_core',
    'wixsetup',
    'nx_vms_client_desktop'
]

verbose = False

DEFAULT = "default"
DEFAULT_SKIN = 'dark_blue'

build_module_prefix = 'build_'

intro_files = {"intro.mkv", "intro.avi", "intro.png", "intro.jpg", "intro.jpeg"}


class Customization():
    def __init__(self, name, root):
        '''
            name - customization name (e.g. default or vista)
            root - path to customization folder (./customization/vista)
        '''
        self.name = name
        self.root = root
        self.config = os.path.join(self.root, 'customization.cmake')


def is_intro(file):
    return any(x for x in intro_files if file.endswith('/' + x))


def find_intro(files):
    return any(x for x in files if is_intro(x))


def is_background(file):
    return file.endswith('/background.png')


def get_files_list(path, recursive=True):
    for dir, _, files in os.walk(path):
        dir = os.path.relpath(dir, path)
        for file in files:
            if not file.startswith("."):
                yield os.path.normpath(os.path.join(dir, file)).replace("\\", "/")
        if not recursive:
            break


# TODO: #GDM Method looks crappy
def detect_module(entry):
    if 'ios' in entry or 'android' in entry or ('mobile' in entry and 'build_mobile' not in entry):
        return 'build_mobile'  # TODO: #GDM fix in 4.0
    if 'paxton' in entry:
        return 'build_paxton'
    if 'nxtool' in entry:
        return 'build_nxtool'


def validate_project(customized, project, mandatory_files, skipped_modules, skin_files):
    customized_files = frozenset(
        x for x in get_files_list(os.path.join(customized.root, project))
        if detect_module(x) not in skipped_modules)
    for file in customized_files - mandatory_files:
        if is_intro(file):
            continue
        if project == 'nx_vms_client_desktop' and file.startswith('resources/skin/'):
            icon = file[len('resources/skin/'):]
            if icon in skin_files:
                continue
        warn('File {0}/{1} is suspicious'.format(project, file))

    for file in customized_files:
        if detect_module(file) in skipped_modules:
            warn('File {0} is suspicious, module {1} is skipped'.format(file, detect_module(file)))

    mandatory_files = frozenset(
        x for x in mandatory_files if detect_module(x) not in skipped_modules)
    for file in mandatory_files - customized_files:
        if is_intro(file) and find_intro(customized_files):
            continue
        if is_background(file):
            continue
        err('File {0}/{1} is missing'.format(project, file))


def validate_config(customized, mandatory_keys, all_keys, skipped_modules):
    customization_keys = frozenset(customized)
    for key in customization_keys - all_keys:
        warn('Key {} is suspicious, probably it is not used'.format(key))

    for key in customization_keys:
        if detect_module(key) in skipped_modules:
            warn('Key {0} is suspicious, module {1} is skipped'.format(key, detect_module(key)))

    mandatory_keys = frozenset(
        x for x in mandatory_keys if detect_module(x) not in skipped_modules)
    for key in mandatory_keys - customization_keys:
        err('Key {} is missing'.format(key))

    return True


def read_customizations(customizations_dir):
    for entry in os.listdir(customizations_dir):
        path = os.path.join(customizations_dir, entry)
        if os.path.isdir(path):
            yield Customization(entry, path)


def validate_skins(root_dir):
    '''
    Main concepts:
    * default skin is dark_blue
    * every icon from the default skin must exist in the other skins
    * no icon from the default skin must exist in the static files
    * every icon from the other skin must exist either in the default skin or in static files
    * every customized icon must exist either in the default skin or in static files
    '''

    client_dir = os.path.join(root_dir, 'vms', 'client', 'nx_vms_client_desktop')
    client_skins_dir = os.path.join(client_dir, 'skins')
    client_static_dir = os.path.join(client_dir, 'static-resources', 'skin')
    default_skin_dir = os.path.join(client_skins_dir, DEFAULT_SKIN, 'skin')
    default_skin = frozenset(x for x in get_files_list(default_skin_dir))
    static_files = frozenset(x for x in get_files_list(client_static_dir))

    for icon in default_skin & static_files:
        warn('Icon {} is present both in skin and static files'.format(icon))

    all_files = default_skin | static_files

    other_skins = {}
    for entry in os.listdir(client_skins_dir):
        if entry == DEFAULT_SKIN:
            continue
        path = os.path.join(client_skins_dir, entry, 'skin')
        if (not os.path.isdir(path)):
            continue
        skin = frozenset(x for x in get_files_list(path))
        other_skins[entry] = skin

    for name, skin in other_skins.iteritems():
        for icon in default_skin - skin:
            warn('Icon {0} is not customized in the {1} skin'.format(icon, name))
        for icon in skin - all_files:
            warn('Icon {0} is not used in the {1} skin'.format(icon, name))
    return all_files


def validate_desktop_client_icons(root_dir, skin_files):
    client_dir = os.path.join(root_dir, 'vms', 'client', 'nx_vms_client_desktop')
    sources = [
        os.path.join(client_dir, 'src'),
        os.path.join(client_dir, 'static-resources', 'src', 'qml')
    ]
    for dir in sources:
        for icon, location in parse_sources_cached(dir):
            if icon in intro_files:
                continue
            if icon not in skin_files:
                warn('Icon {0} is not found in skin (used in {1})'.format(icon, location))


def validate_root(customization, mandatory_files):
    customized_files = frozenset(get_files_list(customization.root, recursive=False))
    for file in customized_files - mandatory_files:
        warn('File {} is suspicious, probably it is not used'.format(file))
    for file in mandatory_files - customized_files:
        err('File {} is missing'.format(file))


def validate_customizations(root_dir):
    if verbose:
        separator()
        info("Validating customizations")

    skin_files = validate_skins(root_dir)
    validate_desktop_client_icons(root_dir, skin_files)

    customizations_dir = os.path.join(root_dir, "customization")
    customizations = list(read_customizations(customizations_dir))
    default = next((c for c in customizations if c.name == DEFAULT))
    if not default:
        err("Critical error: Default customization is not found!")
        return -1

    base_config = read_cmake_config(os.path.join(customizations_dir, 'default-values.cmake'))
    base_keys = frozenset(base_config)
    default_config = read_cmake_config(default.config)
    default_keys = frozenset(default_config)
    mandatory_keys = default_keys - base_keys
    all_keys = default_keys | base_keys
    build_submodules = list(x for x in base_keys if x.startswith(build_module_prefix))
    mandatory_root_files = frozenset(get_files_list(default.root, recursive=False))

    default_project_files = {}
    for project in customizable_projects:
        default_project_files[project] = frozenset(
            get_files_list(os.path.join(default.root, project)))

    for c in customizations:
        if c == default:
            continue

        info('Customization: {0}'.format(c.name))

        validate_root(c, mandatory_root_files)

        config = read_cmake_config(c.config)
        skipped_modules = set()
        for module in build_submodules:
            if config.get(module, base_config.get(module)) == 'OFF':
                skipped_modules.add(module)
        validate_config(config, mandatory_keys, all_keys, skipped_modules)
        for project in customizable_projects:
            validate_project(
                customized=c,
                project=project,
                mandatory_files=default_project_files[project],
                skipped_modules=skipped_modules,
                skin_files=skin_files)

        if verbose:
            separator()

    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--color', action='store_true', help="colorized output")
    parser.add_argument('-v', '--verbose', action='store_true', help="verbose output")
    parser.add_argument('--clear-cache', action='store_true', help="Force clear sources cache")
    args = parser.parse_args()
    if args.color:
        init_color()

    if args.clear_cache:
        clear_sources_cache()

    global verbose
    verbose = args.verbose

    root_dir = os.getcwd()
    return validate_customizations(root_dir)


if __name__ == "__main__":
    result = main()
    sys.exit(result)
