import hashlib
import json
import logging
import zipfile

from flask import Flask, request, send_file
from pathlib2 import PurePosixPath, PureWindowsPath
from werkzeug.exceptions import BadRequest, NotFound, SecurityError

from framework.installation.installer import Version, known_customizations
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script
from framework.os_access.windows_power_shell_utils import power_shell_augment_script

_logger = logging.getLogger(__name__)

UPDATE_PATH_PATTERN = '/{}/{}/update.json'


def _create_install_sh(dir, callback_url, data):
    path = PurePosixPath('install.sh')
    command = ['curl', '--verbose', '--get', callback_url]
    for key, value in data.items():
        command.append('-data-urlencode')
        command.append('{}={}'.format(key, value))
    bare = sh_command_to_script(command)
    full = sh_augment_script(bare, set_eux=False, shebang=True)
    abs_path = dir / path
    abs_path.write_bytes(full)
    abs_path.chmod(0o755)
    return path


def _create_install_ps1(dir, callback_url, data):
    path = PureWindowsPath('install.ps1')
    abs_path = dir / path
    abs_path.write_text(power_shell_augment_script(
        # language=PowerShell
        u'Invoke-WebRequest -Uri $url -UseBasicParsing -Body $data',
        {'url': callback_url, 'data': data}))
    return path


_script_factories = {
    'linux': _create_install_sh,
    'macosx': _create_install_sh,
    'windows': _create_install_ps1,
    }


def _create_archive(archive_path, callback_url, product, customization, cloud_host, version, platform, mod, arch):
    # Archives are made unique intentionally to make their sizes and MD5 hashes different.
    # Files are created to make debug easy and to let Flask serve them with all features.
    unpacked_path = archive_path.with_suffix('')
    unpacked_path.mkdir(parents=True, exist_ok=True)
    callback_data = {
        'product': product,
        'customization': customization.customization_name,
        'version': platform,
        }
    with zipfile.ZipFile(str(archive_path), 'w') as zf:
        _create_install_script = _script_factories[platform]
        script_rel_path = _create_install_script(unpacked_path, callback_url, callback_data)
        zf.write(str(unpacked_path / script_rel_path), str(script_rel_path))
        update_info = {
            'version': str(version),
            'platform': platform,
            'modification': mod,
            'arch': arch,
            'cloudHost': cloud_host,
            'executable': str(script_rel_path),
            }
        zf.writestr('update.json', json.dumps(update_info, indent=4))  # Mediaserver looks for it.


def _calculate_md5_of_file(path):
    hash = hashlib.md5()
    with path.open('rb') as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                return hash.hexdigest()
            hash.update(chunk)


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

    def __init__(self, data_dir, base_url, callback_url):
        self.data_dir = data_dir
        self.root_path = self.data_dir / 'updates.json'
        self.root = {}
        self._base_url = base_url
        self._callback_url = callback_url

    def save_root(self):
        self.root['__version'] = 1
        self.root['__info'] = [{}]
        write_json(self.root_path, self.root)

    def add_customization(self, customization):
        self.root[customization.customization_name] = {
            'releases': {},
            'updates_prefix': self._base_url + '/' + customization.customization_name,
            }

    def add_release(self, customization, version, cloud_host='cloud-test.hdw.mx'):
        _logger.info("Create %s %s", customization.customization_name, version)
        self.root[customization.customization_name]['releases'][version.short_as_str] = version.as_str
        release_dir = self.data_dir / customization.customization_name / str(version.build)
        release_dir.mkdir(parents=True, exist_ok=True)
        packages = {}
        for product, family_to_platforms in self._platforms.items():
            packages[product] = {}
            # "mod" is for "modification".
            for platform, archs_and_mods in family_to_platforms.items():
                packages[product][platform] = {}
                for arch_and_mod in archs_and_mods:
                    archive_name = '{}-{}_update-{}-{}.zip'.format(
                        customization.installer_name,
                        self._product_infix[product],
                        version,
                        self._platform_infix[arch_and_mod],
                        )
                    arch, mod = arch_and_mod.split('_')
                    archive_path = release_dir / archive_name
                    _create_archive(
                        archive_path, self._callback_url,
                        product, customization, cloud_host, version, platform, mod, arch)
                    packages[product][platform][arch_and_mod] = {
                        'file': archive_name,
                        'size': archive_path.stat().st_size,
                        'md5': _calculate_md5_of_file(archive_path),
                        }

        update_path = release_dir / 'update.json'
        packages['cloudHost'] = cloud_host
        write_json(update_path, packages)


class UpdatesServer(object):
    _callback_endpoint = '/.install'

    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.callback_requests = []
        self.download_requests = []

    def generate_data(self, base_url):
        updates = _UpdatesData(self.data_dir, base_url, base_url + self._callback_endpoint)
        for customization in known_customizations:
            updates.add_customization(customization)
            for version in {Version('3.1.0.16975'), Version('3.2.0.17000'), Version('4.0.0.21200')}:
                updates.add_release(customization, version)
        updates.save_root()

    def make_app(self, serve_update_archives, range_header_policy):
        app = Flask(__name__)

        @app.route(self._callback_endpoint)
        def install_callback():
            self.callback_requests.append(request.args)
            return '', 204

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
                self.download_requests.append(path)
                return send_file(
                    str(path),
                    as_attachment=True, attachment_filename=path.name,
                    conditional=range_header_policy == 'support')
            raise NotFound()

        return app


def make_base_url_for_remote_machine(os_access, local_port):
    """Convenience function for fixtures"""
    # TODO: Refactor to get it out of here.
    port_map_local = os_access.port_map.local
    address_from_there = port_map_local.address
    port_from_there = port_map_local.tcp(local_port)
    url = 'http://{}:{}'.format(address_from_there, port_from_there)
    return url
