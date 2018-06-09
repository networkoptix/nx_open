import copy
import json
import logging
import shutil
import sys

from flask import Flask, request, send_file
from werkzeug.exceptions import BadRequest, NotFound, SecurityError

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from httplib import HTTPConnection
elif sys.version_info[:2] in {(3, 4), (3, 5), (3, 6)}:  # Python 3.4 is on Ubuntu 14.04.
    # noinspection PyCompatibility,PyUnresolvedReferences
    from http.client import HTTPConnection

_logger = logging.getLogger(__name__)

UPDATE_PATH_PATTERN = '/{}/{}/update.json'


def write_json(path, data):
    """In Python 2.7, 3.4 and 3.5 json.dumps returns different types."""
    data_json = json.dumps(data, indent=2)
    if isinstance(data_json, bytes):
        path.write_bytes(data_json)
    else:
        path.write_text(data_json)


def read_json(path):
    """In Python 2.7, 3.4 and 3.5 json.loads accepts different types."""
    if str == bytes:
        return json.loads(path.read_bytes())
    else:
        return json.loads(path.read_text())


class UpdatesServer(object):
    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.dummy_file_path = data_dir / 'dummy.zip'

    @staticmethod
    def _collect_actual_data():
        base_url = 'updates.networkoptix.com'

        conn = HTTPConnection(base_url, port=80)
        conn.request('GET', '/updates.json')
        root_obj = json.loads(conn.getresponse().read().decode())

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
                response_contents = response.read().decode()
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

    @staticmethod
    def _append_new_versions(root_obj, path_to_update_obj, new_versions):
        for customization_name, customization_obj in root_obj.items():
            # Searching for the update object with the highest build number for the current customization.
            new_update_obj = None
            highest_build = 0
            for path in path_to_update_obj.keys():
                path_parts = path.split('/')
                if customization_name == path_parts[1] and int(path_parts[2]) > highest_build:
                    if 'packages' in path_to_update_obj[path]:
                        highest_build = int(path_parts[2])
                        new_update_obj = copy.deepcopy(path_to_update_obj[path])

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

    def _save_data_to_files(self, root_obj, path_to_update_obj):
        shutil.rmtree(str(self.data_dir), ignore_errors=True)
        self.data_dir.mkdir()
        write_json(self.data_dir / 'updates.json', root_obj)
        for key, value in path_to_update_obj.items():
            file_path = self.data_dir / key.lstrip('/')
            file_path.parent.mkdir(parents=True, exist_ok=True)
            write_json(file_path, value)

    def generate_data(self):
        _logger.info('Loading and generating data. Be patient')
        root_obj, path_to_update_obj = self._collect_actual_data()
        new_versions = [('4.0', '4.0.0.21200', 'cloud-test.hdw.mx')]
        new_root, new_path_to_update_obj = self._append_new_versions(root_obj, path_to_update_obj, new_versions)
        self._save_data_to_files(new_root, new_path_to_update_obj)
        if not self.dummy_file_path.exists():
            with self.dummy_file_path.open('wb') as f:
                f.seek(1024 * 1024 * 100)
                f.write(b'\0')

    def make_app(self, serve_update_archives, range_header_policy):
        app = Flask(__name__)

        @app.route('/<path:path>')
        def serve(path):
            path = self.data_dir.joinpath(path).resolve()
            if self.data_dir not in path.parents:
                raise SecurityError("Resolved path is outside of data dir.")
            if 'Range' in request.headers and range_header_policy == 'err':
                raise BadRequest('Range header is not supported')
            if path.suffix == '.json':
                if path.exists():
                    return send_file(str(path))
            elif path.suffix == '.zip' and serve_update_archives:
                update_path = path.with_name('update.json')
                if update_path.exists():
                    update = read_json(update_path)
                    for packages_key in {'packages', 'clientPackages'}:
                        for os_specific_packages in update[packages_key].values():
                            for package in os_specific_packages.values():
                                if package['file'] == path.name:
                                    return send_file(
                                        str(self.dummy_file_path),
                                        as_attachment=True, attachment_filename=path.name,
                                        conditional=range_header_policy == 'support')
            raise NotFound()

        return app
