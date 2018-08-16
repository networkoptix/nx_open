from contextlib import closing

import pytest
from netaddr import IPAddress
from pathlib2 import Path

from defaults import defaults
from framework.camera import CameraPool, SampleMediaFile, SampleTestCameraStream
from framework.threaded import ThreadedCall


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
def camera_pool(request, bin_dir, vm_address, sample_media_file, service_ports):
    stream_path = bin_dir / request.config.getoption('--media-stream-path')
    assert stream_path.exists(), '%s is expected at %s' % (stream_path.name, stream_path.parent)
    duration = sample_media_file.duration.total_seconds()
    stream_sample = SampleTestCameraStream(stream_path.read_bytes(), duration)
    with closing(CameraPool(stream_sample, service_ports[0], service_ports[99])) as camera_pool:
        with ThreadedCall(camera_pool.serve, camera_pool.terminate):
            yield camera_pool


@pytest.fixture()
def camera(camera_pool):
    return camera_pool.add_camera('TestCameraLive', '11:22:33:44:55:66')


@pytest.fixture()
def sample_media_file(request, bin_dir):
    path = bin_dir / request.config.getoption('--media-sample-path')
    assert path.exists(), '%s is expected at %s' % (path.name, path.parent)
    return SampleMediaFile(path)
