import json
import logging
import shutil
import sys
from textwrap import dedent

import click
from flask import Flask, request, send_file
from pathlib2 import Path, PurePath
from werkzeug.exceptions import BadRequest, NotFound, SecurityError

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from httplib import HTTPConnection
elif sys.version_info[:2] in {(3, 5), (3, 6)}:
    # noinspection PyCompatibility,PyUnresolvedReferences
    from http.client import HTTPConnection

_logger = logging.getLogger(__name__)

UPDATE_PATH_PATTERN = '/{}/{}/update.json'
UPDATES_PATH = '/updates.json'

DATA_DIR = Path(__file__).parent / 'data'
DUMMY_FILE_NAME = 'dummy.raw'
DUMMY_FILE_PATH = DATA_DIR / DUMMY_FILE_NAME
UPDATES_FILE_NAME = 'updates.json'


def collect_actual_data():
    base_url = 'updates.networkoptix.com'

    conn = HTTPConnection(base_url, port=80)
    conn.request('GET', UPDATES_PATH)
    root_obj = json.loads(conn.getresponse().read())

    path_to_update_obj = dict()
    for customization in root_obj:
        if customization == '__info':
            continue
        releases_obj = root_obj[customization]['releases']
        for release in releases_obj:
            build_num = releases_obj[release].split('.')[-1]
            update_path = UPDATE_PATH_PATTERN.format(customization, build_num)
            conn.request('GET', update_path)
            response = conn.getresponse()
            response_contents = response.read()
            if response.status != 200:
                _logger.warning("Ignore %s: HTTP status code %d.", update_path, response.status)
                continue
            try:
                update_metadata = json.loads(response_contents)
            except ValueError:
                _logger.warning("Ignore %s: not a valid JSON.", update_path)
                continue
            path_to_update_obj[update_path] = update_metadata

    return root_obj, path_to_update_obj


def append_new_versions(root_obj, path_to_update_obj, new_versions):
    for customization_name, customization_obj in root_obj.items():
        # Searching for the update object with the highest build number for the current customization.
        new_update_obj = None
        highest_build = 0
        for path in path_to_update_obj.keys():
            path_parts = path.split('/')
            if customization_name == path_parts[1] and int(path_parts[2]) > highest_build:
                if 'packages' in path_to_update_obj[path]:
                    highest_build = int(path_parts[2])
                    new_update_obj = path_to_update_obj[path]

        if not new_update_obj:
            continue

        for new_version in new_versions:
            customization_obj['releases'][new_version[0]] = new_version[1]
            new_update_obj['version'] = new_version[1]
            new_update_obj['cloudHost'] = new_version[2]

            def amend_file_info_obj(file_info_obj):
                # TODO: amend md5 and size as well
                # Amending file name. Changing only the version part of the file name.
                old_file_name_splits = file_info_obj['file'].split('.')
                new_version_splits = new_version[1].split('.')
                new_file_name_prefix = list(old_file_name_splits[0])
                new_file_name_prefix[-1] = new_version_splits[0]
                new_file_name_prefix = ''.join(new_file_name_prefix)
                new_file_name = '.'.join([
                    new_file_name_prefix,
                    new_version_splits[1], new_version_splits[2], new_version_splits[3],
                    'zip'])
                file_info_obj['file'] = new_file_name

            def amend_packages(update_obj, packages_name):
                for os_key, os_obj in update_obj[packages_name].items():
                    for file_key, file_obj in os_obj.items():
                        amend_file_info_obj(file_obj)

            amend_packages(new_update_obj, 'clientPackages')
            amend_packages(new_update_obj, 'packages')

            new_path_key = UPDATE_PATH_PATTERN.format(customization_name, new_version[1].split('.')[-1])
            path_to_update_obj[new_path_key] = new_update_obj

    return root_obj, path_to_update_obj


def save_data_to_files(root_obj, path_to_update_obj):
    shutil.rmtree(str(DATA_DIR), ignore_errors=True)
    DATA_DIR.mkdir()
    file_path = DATA_DIR / UPDATES_FILE_NAME
    file_path.write_bytes(json.dumps(root_obj))
    for key, value in path_to_update_obj.items():
        file_path = DATA_DIR / key.lstrip('/')
        file_path.parent.mkdir(parents=True, exist_ok=True)
        file_path.write_bytes(json.dumps(value, indent=2))


@click.group(help='Test updates server', epilog=dedent("""
    Replicates functionality of updates.networkoptix.com but
    appends new versions and cloud host information and
    corresponding specific updates links to the existing ones.
    You may specify versions to add as arguments of append_new_versions(...)."""))
def main():
    pass


@main.command(short_help="Collect data, add versions", help=dedent("""
    Collect actual data from updates.networkoptix.com, add versions and save to the data dir."""))
def generate():
    _logger.info('Loading and generating data. Be patient')
    root_obj, path_to_update_obj = collect_actual_data()
    new_versions = [('4.0', '4.0.0.21200', 'cloud-test.hdw.mx')]
    new_root, new_path_to_update_obj = append_new_versions(root_obj, path_to_update_obj, new_versions)
    save_data_to_files(new_root, new_path_to_update_obj)
    with DUMMY_FILE_PATH.open('wb') as f:
        f.seek(1024 * 1024 * 100)
        f.write(b'\0')


@main.command(short_help="Start HTTP server", help="Serve update metadata and, optionally, archives by HTTP.")
@click.option('--serve-update-archives/--no-serve-update-archives', default=True, show_default=True, help=dedent("""
    Server can serve update files requests. Pre-generated dummy file is given as a response.
    This option might be turned off to test how mediaserver deals with update files absence."""))
@click.option('--range-header', type=click.Choice(['support', 'ignore', 'err']), default='support', show_default=True)
def serve(serve_update_archives, range_header):
    app = Flask(__name__)

    @app.route('/<path:path>')
    def serve(path):
        path = PurePath(path)

        if 'Range' in request.headers and range_header == 'err':
            raise BadRequest('Range header is not supported')

        if path.suffix == '.json':
            return send_file(str(DATA_DIR / path))
        elif path.suffix == '.zip' and not serve_update_archives:
            possible_path_key = DATA_DIR / path.parent / 'update.json'

            if DATA_DIR not in possible_path_key.resolve().parents:
                raise SecurityError()
            if not possible_path_key.exists():
                raise NotFound()

            update_obj = json.loads(possible_path_key.read_bytes())

            def file_found_in_package(packages_name):
                for os_key, os_obj in update_obj[packages_name].items():
                    for file_key, file_obj in os_obj.items():
                        if file_obj['file'] == path.name:
                            return True
                return False

            if not file_found_in_package('packages') and not file_found_in_package('clientPackages'):
                raise NotFound()

            if not DUMMY_FILE_PATH.exists():
                raise Exception('Dummy file not found')

            return send_file(
                str(DUMMY_FILE_PATH),
                as_attachment=True, attachment_filename=path.name,
                conditional=range_header == 'support')
        else:
            raise NotFound()

    app.run(port=8080)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    main()
