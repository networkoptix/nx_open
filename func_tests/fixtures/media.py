import pytest
from netaddr import IPAddress
from pathlib2 import Path

from defaults import defaults
from framework.camera import CameraFactory, SampleMediaFile


def pytest_addoption(parser):
    parser.addoption('--media-sample-path', type=Path, default=defaults['media_sample_path'], help=(
        'media sample file path, default is %(default)s at binary directory'))
    parser.addoption('--media-stream-path', type=Path, default=defaults['media_stream_path'], help=(
        'media sample test camera stream, relative to bin dir [%(default)s]'))
    parser.addoption('--vm-address', type=IPAddress, help=(
        'IP address virtual machines bind to. '
        'Test camera discovery will answer only to this address if this option is specified.'))


@pytest.fixture(params=['rtsp', 'webm', 'hls', 'direct-hls'])
def stream_type(request):
    return request.param


@pytest.fixture(scope='session')
def vm_address(request):
    return request.config.getoption('--vm-address')


@pytest.fixture()
def camera_factory(request, bin_dir, vm_address):
    stream_path = bin_dir / request.config.getoption('--media-stream-path')
    assert stream_path.exists(), '%s is expected at %s' % (stream_path.name, stream_path.parent)
    factory = CameraFactory(vm_address, stream_path)
    yield factory
    factory.close()


@pytest.fixture()
def camera(camera_factory):
    return camera_factory()


@pytest.fixture()
def sample_media_file(request, bin_dir):
    path = bin_dir / request.config.getoption('--media-sample-path')
    assert path.exists(), '%s is expected at %s' % (path.name, path.parent)
    return SampleMediaFile(path)
