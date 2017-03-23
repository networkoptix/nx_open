import os.path
from datetime import datetime, timedelta
import time
import pytest
import pytz
from test_utils.utils import log_list
from test_utils.server import TimePeriod


@pytest.fixture(params=['rtsp', 'webm', 'hls', 'direct-hls'])
def stream_type(request):
    return request.param

@pytest.fixture
def env(env_builder, server):
    server = server()
    return env_builder(server=server)

# https://networkoptix.atlassian.net/browse/TEST-181
def test_media_stream_should_be_loaded_correctly(env, camera, sample_media_file, stream_type):
    # prepare media archive
    camera_id = env.server.add_camera(camera)
    start_time = datetime(2017, 1, 27, tzinfo=pytz.utc)
    env.server.storage.save_media_sample(camera, start_time, sample_media_file)
    env.server.rebuild_archive()
    assert [TimePeriod(start_time, sample_media_file.duration)] == env.server.get_recorded_time_periods(camera_id)

    # load stream
    stream = env.server.get_media_stream(stream_type, camera)
    metadata_list = stream.load_archive_stream_metadata(
        os.path.join(env.work_dir, 'stream-media-%s' % stream_type),
        pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height

    camera_info_list = env.server.rest_api.ec2.getCamerasEx.GET()
    # todo: check mediastream atrributes after test camera support is added
