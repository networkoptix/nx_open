import os.path
from datetime import datetime, timedelta
import time
import pytest
import pytz
from test_utils.utils import log_list
from test_utils.server import TimePeriod


# https://networkoptix.atlassian.net/browse/TEST-181
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920667/Media+stream+loading+test
def test_media_stream_should_be_loaded_correctly(artifact_file, server_factory, camera, sample_media_file, stream_type):
    server = server_factory('server')
    # prepare media archive
    server.add_camera(camera)
    start_time = datetime(2017, 1, 27, tzinfo=pytz.utc)
    server.storage.save_media_sample(camera, start_time, sample_media_file)
    server.rebuild_archive()
    assert [TimePeriod(start_time, sample_media_file.duration)] == server.get_recorded_time_periods(camera)

    # load stream
    stream = server.get_media_stream(stream_type, camera)
    path = artifact_file('stream-media', stream_type)
    metadata_list = stream.load_archive_stream_metadata(
        path, pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height
