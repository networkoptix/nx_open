import pytest
from pathlib2 import Path

from framework.camera import CameraFactory, SampleMediaFile

MEDIA_SAMPLE_PATH = 'sample.mkv'
MEDIA_STREAM_PATH = 'sample.testcam-stream.data'


def pytest_addoption(parser):
    parser.addoption('--media-sample-path', type=Path, default=MEDIA_SAMPLE_PATH, help=(
        'media sample file path, default is %s at binary directory' % MEDIA_SAMPLE_PATH))
    parser.addoption('--media-stream-path', type=Path, default=MEDIA_STREAM_PATH, help=(
        'media sample test camera stream, relative to bin dir [%s]' % MEDIA_STREAM_PATH))


@pytest.fixture(params=['rtsp', 'webm', 'hls', 'direct-hls'])
def stream_type(request):
    return request.param


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
