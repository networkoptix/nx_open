from datetime import datetime

import pytz

from framework.server import TimePeriod


# https://networkoptix.atlassian.net/browse/TEST-181
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920667/Media+stream+loading+test
def test_media_stream_should_be_loaded_correctly(artifact_factory, running_linux_server, camera, sample_media_file, stream_type):
    # prepare media archive
    running_linux_server.add_camera(camera)
    start_time = datetime(2017, 1, 27, tzinfo=pytz.utc)
    running_linux_server.storage.save_media_sample(camera, start_time, sample_media_file)
    running_linux_server.rebuild_archive()
    assert [TimePeriod(start_time, sample_media_file.duration)] == running_linux_server.get_recorded_time_periods(camera)

    # load stream
    stream = running_linux_server.get_media_stream(stream_type, camera)
    metadata_list = stream.load_archive_stream_metadata(
        artifact_factory(['stream-media', stream_type]),
        pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height
    assert not running_linux_server.installation.list_core_dumps()
