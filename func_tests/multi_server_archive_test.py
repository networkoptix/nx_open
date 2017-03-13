from datetime import datetime, timedelta
import logging
import uuid
import pytest
import pytz
from test_utils import print_list
from server import TimePeriod


log = logging.getLogger(__name__)


@pytest.fixture
def env(env_builder, server):
    one = server()
    two = server()
    return env_builder(merge_servers=[one, two], one=one, two=two)


def test_merged_archive(env, camera, sample_media_file):
    log.debug('camera: %r, sample media file: %r', camera, sample_media_file)
    camera_id = env.one.add_camera(camera)
    one_storage = env.one.storage
    two_storage = env.two.storage
    sample = sample_media_file
    log.debug('Sample duration: %s', sample.duration)

    start_times_one = []
    start_times_one.append(datetime(2017, 1, 27, tzinfo=pytz.utc))
    start_times_one.append(start_times_one[-1] + sample.duration - timedelta(seconds=10))  # overlapping with previous
    start_times_one.append(start_times_one[-1] + sample.duration + timedelta(minutes=1))   # separate from previous
    start_times_two = []
    start_times_two.append(start_times_one[-1] + sample.duration + timedelta(minutes=1))   # separate from previous
    start_times_two.append(start_times_two[-1] + sample.duration)                          # adjacent to previous
    print_list('Start times for server one', start_times_one)
    print_list('Start times for server two', start_times_two)
    expected_periods_one = [
        TimePeriod(start_times_one[0], sample.duration * 2 - timedelta(seconds=10)),  # overlapped must be joined together
        TimePeriod(start_times_one[2], sample.duration),
        ]
    expected_periods_two = [
        TimePeriod(start_times_two[0], sample.duration * 2),  # adjacent must be joined together
        ]
    all_expected_periods = expected_periods_one + expected_periods_two
    print_list('Expected periods for server one', expected_periods_one)
    print_list('Expected periods for server two', expected_periods_two)

    for st in start_times_one:
        one_storage.save_media_sample(camera, st, sample)
    for st in start_times_two:
        two_storage.save_media_sample(camera, st, sample)
    env.one.rebuild_archive()
    env.two.rebuild_archive()
    assert all_expected_periods == env.one.get_recorded_time_periods(camera_id)
    assert all_expected_periods == env.two.get_recorded_time_periods(camera_id)
    return (camera_id, expected_periods_one, expected_periods_two)


def test_separated_archive(env, camera, sample_media_file):
    camera_id, expected_periods_one, expected_periods_two = test_merged_archive(env, camera, sample_media_file)
    env.one.change_system_id('{%s}' % uuid.uuid4())
    assert expected_periods_one == env.one.get_recorded_time_periods(camera_id)
    assert expected_periods_two == env.two.get_recorded_time_periods(camera_id)
