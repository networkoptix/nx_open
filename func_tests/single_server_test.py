from datetime import datetime, timedelta
import time
import pytest
import pytz
from test_utils.utils import log_list
from test_utils.server import TimePeriod


@pytest.fixture
def env(env_builder, server):
    server = server()
    return env_builder(server=server)


# https://networkoptix.atlassian.net/browse/VMS-3911
def test_server_should_pick_archive_file_with_time_after_db_time(env, camera, sample_media_file):
    camera_id = env.server.add_camera(camera)
    storage = env.server.storage
    sample = sample_media_file

    start_times_1 = []
    start_times_1.append(datetime(2017, 1, 27, tzinfo=pytz.utc))
    start_times_1.append(start_times_1[-1] + sample.duration + timedelta(minutes=1))
    start_times_2 = []
    start_times_2.append(start_times_1[-1] + sample.duration + timedelta(minutes=1))
    start_times_2.append(start_times_2[-1] + sample.duration + timedelta(minutes=1))
    expected_periods_1 = [
        TimePeriod(start_times_1[0], sample.duration),
        TimePeriod(start_times_1[1], sample.duration),
        ]
    expected_periods_2 = [
        TimePeriod(start_times_2[0], sample.duration),
        TimePeriod(start_times_2[1], sample.duration),
        ]
    log_list('Start times 1', start_times_1)
    log_list('Start times 2', start_times_2)
    log_list('Expected periods 1', expected_periods_1)
    log_list('Expected periods 2', expected_periods_2)

    for st in start_times_1:
        storage.save_media_sample(camera, st, sample)
    env.server.rebuild_archive()
    assert expected_periods_1 == env.server.get_recorded_time_periods(camera_id)

    # stop service and add more media files to archive:
    env.server.stop_service()
    for st in start_times_2:
        storage.save_media_sample(camera, st, sample)
    env.server.start_service()

    time.sleep(10)  # servers still need some time to settle down; hope this time will be enough
    # after restart new periods must be picked:
    recorded_periods = env.server.get_recorded_time_periods(camera_id)
    assert recorded_periods != expected_periods_1, 'Server did not pick up new media archive files'
    assert expected_periods_1 + expected_periods_2 == recorded_periods
