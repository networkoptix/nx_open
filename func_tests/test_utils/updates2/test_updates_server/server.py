"""
Test updates server. Replicates functionality of updates.networkoptix.com but appends new versions and cloud host
information and corresponding specific updates links to the existing ones. You may specify what to add in the call to
the append_new_versions() function (see main() for example).
If started with '--generate_data' key collects actual data from updates.networkoptix.com, makes needed amendments and
saves result to the 'data' folder before HTTP server start. Without that key - tries to read data from the 'data'
folder.
Server can serve update files requests. Pre-generated dummy file is given as a response. This option might be turned off
to test how mediaserver deals with update files absence. Run script with '--emulate_no_update_files' key to achieve
this behavior.
"""

import argparse
import json
import logging
import shutil
import sys

from pathlib2 import Path

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
    # noinspection PyCompatibility,PyUnresolvedReferences
    from httplib import HTTPConnection
elif sys.version_info[:2] in {(3, 5), (3, 6)}:
    # noinspection PyCompatibility,PyUnresolvedReferences
    from http.server import HTTPServer, BaseHTTPRequestHandler
    # noinspection PyCompatibility,PyUnresolvedReferences
    from http.client import HTTPConnection

_logger = logging.getLogger(__name__)

UPDATE_PATH_PATTERN = '/{}/{}/update.json'
UPDATES_PATH = '/updates.json'

DATA_FOLDER_NAME = 'data'
DATA_DIR = Path(__file__).parent / 'data'
DUMMY_FILE_NAME = 'dummy.raw'
DUMMY_FILE_PATH = DATA_DIR / DUMMY_FILE_NAME
UPDATES_FILE_NAME = 'updates.json'
FILE_PATTERN = '{}.{}.json'


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
            content = response.read()
            if response.status != 200:
                continue
            try:
                path_to_update_obj[update_path] = json.loads(content)
            except Exception as e:
                # Some updates might be not valid jsons, we are just not interested in them
                pass

    return root_obj, path_to_update_obj


def append_new_versions(root_obj, path_to_update_obj, new_versions):
    for customization_name, customization_obj in root_obj.items():
        # Searching for the update object with the highest build number for the current customization.
        new_update_obj = None
        highest_build = 0
        for path in path_to_update_obj.keys():
            path_parts = path.split('/')
            if customization_name == path_parts[1] and int(path_parts[2]) > highest_build and \
                    'packages' in path_to_update_obj[path]:
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
                new_file_name = '.'.join([new_file_name_prefix, new_version_splits[1], new_version_splits[2],
                                          new_version_splits[3], 'zip'])
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
        key_splits = key.split('/')
        file_path = DATA_DIR / FILE_PATTERN.format(key_splits[1], key_splits[2])
        file_path.write_bytes(json.dumps(value, indent=2))


def load_data_from_files():
    if not DATA_DIR.exists():
        raise Exception('Generated data directory has not been found')
    root_obj, path_to_update = None, {}
    for file_path in DATA_DIR.glob('**/*.json'):
        json_obj = json.loads(file_path.read_bytes())
        if file_path.name == UPDATES_FILE_NAME:
            root_obj = json_obj
        else:
            file_name_splits = file_path.name.split('.')
            update_path = UPDATE_PATH_PATTERN.format(file_name_splits[0], file_name_splits[1])
            path_to_update[update_path] = json_obj
    if root_obj is None or not path_to_update:
        raise Exception('Invalid generated data')
    return root_obj, path_to_update


def parse_args():
    parser = argparse.ArgumentParser(description='Test updates server')
    parser.add_argument('--generate_data', action="store_true", default=False)
    parser.add_argument('--emulate_no_update_files', action="store_true", default=False)
    return parser.parse_args()


def make_handler_class(root_obj, path_to_update, args):
    path_to_update[UPDATES_PATH] = root_obj

    class TestHandler(BaseHTTPRequestHandler, object):
        def __init__(self, *args, **kwargs):
            self.path_to_update = path_to_update
            super(TestHandler, self).__init__(*args, **kwargs)

        def _send_ok_headers(self, content_type, content_length=None):
            self.send_response(200)
            self.send_header('Content-type', content_type)
            if content_length:
                self.send_header('Content-length', content_length)
            self.end_headers()

        def _send_not_found(self):
            self.send_response(404)
            self.end_headers()

        def do_GET(self):
            self._serve_get(True)

        def do_HEAD(self):
            self._serve_get(False)

        def _serve_get(self, give_away_content):
            if self.path in self.path_to_update:
                self._send_ok_headers('application/json')
                if give_away_content:
                    self.wfile.write(json.dumps(self.path_to_update[self.path]).encode())
            elif self.path.endswith('.zip') and not args.emulate_no_update_files:
                path_components = self.path.split('/')
                possible_path_key = '/'.join(path_components[:-1]) + '/update.json'
                requested_file_name = path_components[-1]

                if possible_path_key not in self.path_to_update:
                    self._send_not_found()
                    return

                update_obj = self.path_to_update[possible_path_key]

                def file_found_in_package(packages_name):
                    for os_key, os_obj in update_obj[packages_name].items():
                        for file_key, file_obj in os_obj.items():
                            if file_obj['file'] == requested_file_name:
                                return True
                    return False

                if not file_found_in_package('packages') and not file_found_in_package('clientPackages'):
                    self._send_not_found()
                    return

                if not DUMMY_FILE_PATH.exists():
                    raise Exception('Dummy file not found')

                self._send_ok_headers('application/zip', DUMMY_FILE_PATH.stat().st_size)
                if not give_away_content:
                    return

                with DUMMY_FILE_PATH.open('rb') as f:
                    shutil.copyfileobj(f, self.wfile, length=4096)
            else:
                self._send_not_found()

    return TestHandler


def main():
    args = parse_args()
    if args.generate_data:
        _logger.info('Loading and generating data. Be patient')
        root_obj, path_to_update_obj = collect_actual_data()
        new_versions = [('4.0', '4.0.0.21200', 'cloud-test.hdw.mx')]
        new_root, new_path_to_update_obj = append_new_versions(root_obj, path_to_update_obj, new_versions)
        save_data_to_files(new_root, new_path_to_update_obj)
        with DUMMY_FILE_PATH.open('wb') as f:
            f.seek(1024 * 1024 * 100)
            f.write(b'\0')
    else:
        new_root, new_path_to_update_obj = load_data_from_files()

    server_address = ('', 8080)
    _logger.info('Starting HTTP server')
    server = None
    try:
        server = HTTPServer(server_address, make_handler_class(new_root, new_path_to_update_obj, args))
        server.serve_forever()
    except KeyboardInterrupt:
        _logger.info('Shutting down...')
        if server:
            server.shutdown()


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    main()
