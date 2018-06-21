import os

import pytest
from flask import Flask, send_file
from werkzeug.exceptions import NotFound

from framework.os_access.exceptions import AlreadyExists
from framework.os_access.local_access import local_access
from framework.serving import WsgiServer, make_base_url_for_remote_machine


@pytest.fixture(scope='session', params=['linux', 'windows', 'local'])
def os_access(request):
    if request.param == 'local':
        return local_access
    else:
        vm_fixture = request.param + '_vm'
        vm = request.getfixturevalue(vm_fixture)
        return vm.os_access


@pytest.fixture()
def served_file(node_dir):
    served_dir = node_dir / 'served_dir'
    served_dir.mkdir(parents=True, exist_ok=True)
    served_file = served_dir / 'served_file.random.dat'
    served_file.write_bytes(os.urandom(100500))
    return served_file


@pytest.fixture()
def downloads_dir(os_access):
    downloads_dir = os_access.Path.tmp() / 'downloads_dir'
    downloads_dir.mkdir(parents=True, exist_ok=True)
    return downloads_dir


@pytest.fixture()
def _http_url(os_access, served_file):
    app = Flask('One file download')

    @app.route('/<path:path>')
    def download(path):
        if path != served_file.name:
            raise NotFound()
        return send_file(str(served_file), as_attachment=True)

    wsgi_server = WsgiServer(app.wsgi_app, range(8081, 8100))
    with wsgi_server.serving():
        base_url = make_base_url_for_remote_machine(os_access, wsgi_server.port)
        yield base_url + '/' + served_file.name


@pytest.fixture()
def _smb_url(request):
    url = request.config.getoption('--dummy-smb-url', skip=True)
    return url


@pytest.fixture()
def _file_url(served_file):
    return 'file://' + str(served_file)


@pytest.fixture(params=['http', 'smb', 'file'])
def url(request):
    return request.getfixturevalue('_' + request.param + '_url')


@pytest.mark.skipifnotimplemented
def test_download(os_access, url, downloads_dir):
    path = os_access.download(url, downloads_dir)
    assert path.exists()
    with pytest.raises(AlreadyExists):
        os_access.download(url, downloads_dir)
