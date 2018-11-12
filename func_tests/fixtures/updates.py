import hashlib

import flask
import pytest
import werkzeug.exceptions

from defaults import defaults
from framework.cloud_host import resolve_cloud_host_from_registry
from framework.installation.installer import InstallerSet
from framework.os_access.local_path import LocalPath
from framework.serving import WsgiServer


def pytest_addoption(parser):
    parser.addoption(
        '--mediaserver-updates-dir',
        default=defaults.get('mediaserver_updates_dir'),
        help="Directory with updates of same version and customization.")


@pytest.fixture()
def updates_dir(request):
    return LocalPath(request.config.getoption('--mediaserver-updates-dir'))


def _calculate_md5_of_file(path):
    hash = hashlib.md5()
    with path.open('rb') as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                return hash.hexdigest()
            hash.update(chunk)


def _make_updates_wsgi_app(updates_dir, range_header_policy='support'):
    """Make app that serves update archives. The only reason to use flask here is to have a
    control over Range header, which may be checked in future."""
    app = flask.Flask(__name__)

    @app.route('/<path:path>')
    def serve(path):
        path = updates_dir.joinpath(path)
        if updates_dir.resolve() not in path.resolve().parents:
            raise werkzeug.exceptions.SecurityError("Resolved path is outside of data dir.")
        if 'Range' in flask.request.headers and range_header_policy == 'err':
            raise werkzeug.exceptions.BadRequest('Range header is not supported')
        return flask.send_file(
            str(path),
            as_attachment=True, attachment_filename=path.name,
            conditional=range_header_policy == 'support')

    return app


@pytest.fixture()
def updates_server_url(service_ports, updates_dir, runner_address):
    app = _make_updates_wsgi_app(updates_dir)

    wsgi_server = WsgiServer(app, service_ports[10:15])
    # When port is bound and it's known how to access server's address and port, generate.
    base_url = 'http://{}:{}'.format(runner_address, wsgi_server.port)
    with wsgi_server.serving():
        yield base_url + '/'


@pytest.fixture()
def updates_set(updates_dir):
    return InstallerSet(updates_dir)


@pytest.fixture()
def update_info(updates_dir, updates_set, updates_server_url):
    return {
        'version': str(updates_set.version) + '1',
        'cloudHost': resolve_cloud_host_from_registry('test', updates_set.customization.customization_name),
        'eulaLink': 'http://new.eula.com/eulaText',
        'eulaVersion': 1,
        'releaseNotesUrl': 'http://www.networkoptix.com/all-nx-witness-release-notes',
        'packages': [
            {
                'component': 'server',
                'arch': installer.arch,
                'platform': installer.platform,
                'variant': installer.platform_variant,
                'variantVersion': installer.platform_variant_version,
                'file': 'updates/{}/{}'.format(installer.version.build, installer.path.name),
                'url': updates_server_url + str(installer.path.relative_to(updates_dir)),
                'size': installer.path.stat().st_size,
                'md5': _calculate_md5_of_file(installer.path),
                }
            for installer in updates_set.installers
            if installer.component == 'server_update'
            ]
        }
