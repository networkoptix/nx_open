"""
Test updates server. Replicates functionality of updates.networkoptix.com but appends new versions and cloud host
information and corresponding specific updates links to the existing ones. You may specify what to add in the call to
the append_new_versions() function (see main() for example).
If started with '--generate_data' key collects actual data from updates.networkoptix.com, makes needed amendments and
saves result to the 'data' folder before HTTP server start. Without that key - tries to read data from the 'data'
folder.
"""

import shutil
from http.server import HTTPServer, BaseHTTPRequestHandler
from http.client import HTTPConnection
import json
import argparse
import os


UPDATE_PATH_PATTERN = '/{}/{}/update.json'
UPDATES_PATH = '/updates.json'

DATA_FOLDER_NAME = 'data'
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
        existing_update_key_with_customization = \
            next(filter(lambda x: customization_name in x, path_to_update_obj.keys()), None)
        if existing_update_key_with_customization:
            if 'packages' not in path_to_update_obj[existing_update_key_with_customization]:
                continue
            for new_version in new_versions:
                customization_obj['releases'][new_version[0]] = new_version[1]
                new_update_obj = path_to_update_obj[existing_update_key_with_customization].copy()
                new_update_obj['version'] = new_version[1]
                new_update_obj['cloudHost'] = new_version[2]

                def amend_file_name(file_info_obj):
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
                            amend_file_name(file_obj)

                amend_packages(new_update_obj, 'clientPackages')
                amend_packages(new_update_obj, 'packages')

                path_to_update_obj[UPDATE_PATH_PATTERN.format(customization_name, new_version[1].split('.')[-1])] = new_update_obj

    return root_obj, path_to_update_obj


def save_data_to_files(root_obj, path_to_update_obj):
    script_path = os.path.dirname(os.path.abspath(__file__))
    data_path = os.path.join(script_path, DATA_FOLDER_NAME)
    shutil.rmtree(data_path, ignore_errors=True)
    os.mkdir(data_path)
    file_path = os.path.join(data_path, UPDATES_FILE_NAME)
    with open(file_path, 'w') as f:
        f.write(json.dumps(root_obj))
    for key, value in path_to_update_obj.items():
        key_splits = key.split('/')
        file_path = os.path.join(data_path, FILE_PATTERN.format(key_splits[1], key_splits[2]))
        with open(file_path, 'w') as f:
            f.write(json.dumps(value))


def load_data_from_files():
    for dir_name, dirs, files in os.walk(os.path.join(os.path.dirname(os.path.abspath(__file__)), DATA_FOLDER_NAME)):
        path_to_update = dict()
        for file_name in files:
            file_path = os.path.join(dir_name, file_name)
            with open(file_path, 'r') as f:
                json_obj = json.loads(f.read())
                if file_name.lower() == UPDATES_FILE_NAME:
                    root_obj = json_obj
                else:
                    file_name_splits = file_name.split('.')
                    update_path = UPDATE_PATH_PATTERN.format(file_name_splits[0], file_name_splits[1])
                    path_to_update[update_path] = json_obj
    return root_obj, path_to_update


def parse_args():
    parser = argparse.ArgumentParser(description='Test updates server')
    parser.add_argument('--generate_data', action="store_true", default=False)
    return parser.parse_args()


def make_handler_class(root_obj, path_to_update):
    path_to_update[UPDATES_PATH] = root_obj

    class TestHandler(BaseHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            self.path_to_update = path_to_update
            super().__init__(*args, **kwargs)

        def _send_ok_headers(self):
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()

        def do_GET(self):
            if self.path in self.path_to_update:
                self._send_ok_headers()
                self.wfile.write(bytes(json.dumps(self.path_to_update[self.path]), encoding='utf-8'))
            else:
                self.send_response(404)
                self.end_headers()

    return TestHandler


def main():
    args = parse_args()
    if args.generate_data:
        print('Loading and generating data. Be patient')
        new_root, new_path_to_update_obj = append_new_versions(*collect_actual_data(),
                                                               [('4.0', '4.0.0.21200', 'cloud-test.hdw.mx')])
        save_data_to_files(new_root, new_path_to_update_obj)
    else:
        try:
            new_root, new_path_to_update_obj = load_data_from_files()
        except Exception as e:
            print(e)
            print('Failed to load test data. Did you forget to generate it first?')
            return

    server_address = ('', 8080)
    print('Starting HTTP server')
    HTTPServer(server_address, make_handler_class(new_root, new_path_to_update_obj)).serve_forever()


if __name__ == '__main__':
    main()