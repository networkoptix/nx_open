from datetime import datetime

import pytz

from framework.mediaserver_api import TimePeriod


# https://networkoptix.atlassian.net/browse/TEST-181
# https://networkoptix.atlassian.net/wiki/spaces/SD/pages/23920667/Media+stream+loading+test
def test_media_stream_should_be_loaded_correctly(
        artifact_factory, one_running_mediaserver, camera, sample_media_file, stream_type):
    # prepare media archive
    one_running_mediaserver.api.add_camera(camera)
    start_time = datetime(2017, 1, 27, tzinfo=pytz.utc)
    one_running_mediaserver.storage.save_media_sample(camera, start_time, sample_media_file)
    one_running_mediaserver.api.rebuild_archive()
    recorded_time_periods = one_running_mediaserver.api.get_recorded_time_periods(camera.id)
    assert [TimePeriod(start_time, sample_media_file.duration)] == recorded_time_periods

    # load stream
    stream = one_running_mediaserver.api.get_media_stream(stream_type, camera.mac_addr)
    metadata_list = stream.load_archive_stream_metadata(
        artifact_factory(['stream-media', stream_type]),
        pos=start_time, duration=sample_media_file.duration)
    for metadata in metadata_list:
        assert metadata.width == sample_media_file.width
        assert metadata.height == sample_media_file.height
    assert not one_running_mediaserver.installation.list_core_dumps()
