import json
import logging
import os
import random

from flask import Flask, request, send_file
import hashlib
from werkzeug.exceptions import BadRequest, NotFound, SecurityError

from framework.installation.installer import Version, known_customizations

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


class _UpdatesData(object):
    _platforms = {
        'packages': {
            'linux': {'x64_ubuntu', 'x86_ubuntu', 'arm_rpi', 'arm_bpi', 'arm_bananapi'},
            'windows': {'x64_winxp', 'x86_winxp'},
            },
        'clientPackages': {
            'linux': {'x64_ubuntu', 'x86_ubuntu'},
            'macosx': {'x64_macosx'},
            'windows': {'x64_winxp', 'x86_winxp'},
            }
        }
    _product_infix = {
        'packages': 'server',
        'clientPackages': 'client',
        }
    _platform_infix = {
        'x64_ubuntu': 'linux64', 'x86_ubuntu': 'linux86',
        'x64_macosx': 'mac',
        'x64_winxp': 'win64', 'x86_winxp': 'win86',
        'arm_rpi': 'rpi', 'arm_bpi': 'bpi', 'arm_bananapi': 'bananapi',
        }

    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.root_path = self.data_dir / 'updates.json'
        self.root = {}

    def save_root(self):
        self.root['__version'] = 1
        self.root['__info'] = [{}]
        write_json(self.root_path, self.root)

    def add_customization(self, customization):
        self.root[customization.customization_name] = {'releases': {}}

    def _make_archive_name(self, customization, product, platform, version):
        return '{}-{}_update-{}-{}.zip'.format(
            customization.installer_name,
            self._product_infix[product],
            version,
            self._platform_infix[platform],
            )

    def add_release(self, customization, version):
        _logger.info("Create %s %s", customization.customization_name, version)
        self.root[customization.customization_name]['releases'][version.short_as_str] = version.as_str
        release_dir = self.data_dir / customization.customization_name / str(version.build)
        release_dir.mkdir(parents=True, exist_ok=True)
        packages = {}
        for product, family_to_platforms in self._platforms.items():
            packages[product] = {}
            for family, platforms in family_to_platforms.items():
                packages[product][family] = {}
                for platform in platforms:
                    archive_name = self._make_archive_name(customization, product, platform, version)
                    packages[product][family][platform] = {
                        'file': archive_name,
                        'size': random.randint(1000000, 2000000),
                        'md5': hashlib.md5(os.urandom(10)).hexdigest(),
                        }
        update_path = release_dir / 'update.json'
        packages['cloudHost'] = 'cloud-test.hdw.mx'
        write_json(update_path, packages)


class UpdatesServer(object):
    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.dummy_file_path = data_dir / 'dummy.zip'

    def generate_data(self):
        updates = _UpdatesData(self.data_dir)
        for customization in known_customizations:
            updates.add_customization(customization)
            for version in {Version('3.1.0.16975'), Version('3.2.0.17000'), Version('4.0.0.21200')}:
                updates.add_release(customization, version)
        updates.save_root()
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
