import os

import flask
import pytest
import werkzeug.exceptions

from framework.os_access.exceptions import AlreadyExists
from framework.serving import WsgiServer


@pytest.fixture()
def served_file(node_dir):
    served_dir = node_dir / 'served_dir'
    served_dir.mkdir(parents=True, exist_ok=True)
    served_file = served_dir / 'served_file.random.dat'
    served_file.write_bytes(os.urandom(100500))
    return served_file


@pytest.fixture()
def downloads_dir(os_access):
    downloads_dir = os_access.path_cls.tmp() / 'downloads_dir'
    downloads_dir.rmtree(ignore_errors=True)
    downloads_dir.mkdir(parents=True)
    return downloads_dir


@pytest.fixture()
def _http_url(service_ports, os_access, served_file):
    app = flask.Flask('One file download')

    @app.route('/<path:path>')
    def download(path):
        if path != served_file.name:
            raise werkzeug.exceptions.NotFound()
        return flask.send_file(str(served_file), as_attachment=True)

    wsgi_server = WsgiServer(app.wsgi_app, service_ports[5:10])
    with wsgi_server.serving():
        base_url = 'http://{}:{}'.format(os_access.port_map.local.address, wsgi_server.port)
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
    assert path.exists() or path.is_symlink()
    with pytest.raises(AlreadyExists) as excinfo:
        os_access.download(url, downloads_dir)
    assert excinfo.value.path == path
