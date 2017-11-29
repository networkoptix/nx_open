import logging
import uuid
from datetime import datetime, timedelta

import pytz

from test_utils.server import TimePeriod
from test_utils.utils import log_list

log = logging.getLogger(__name__)


def test_merged_archive(server_factory, camera, sample_media_file):
    log.debug('camera: %r, sample media file: %r', camera, sample_media_file)

    one = server_factory('one')
    two = server_factory('two')
    one.merge([two])

    one.add_camera(camera)

    sample = sample_media_file
    log.debug('Sample duration: %s', sample.duration)

    start_times_one = []
    start_times_one.append(datetime(2017, 1, 27, tzinfo=pytz.utc))
    start_times_one.append(start_times_one[-1] + sample.duration - timedelta(seconds=10))  # overlapping with previous
    start_times_one.append(start_times_one[-1] + sample.duration + timedelta(minutes=1))   # separate from previous
    start_times_two = []
    start_times_two.append(start_times_one[-1] + sample.duration + timedelta(minutes=1))   # separate from previous
    start_times_two.append(start_times_two[-1] + sample.duration)                          # adjacent to previous
    log_list('Start times for server one', start_times_one)
    log_list('Start times for server two', start_times_two)
    expected_periods_one = [
        TimePeriod(start_times_one[0], sample.duration * 2 - timedelta(seconds=10)),  # overlapped must be joined together
        TimePeriod(start_times_one[2], sample.duration),
        ]
    expected_periods_two = [
        TimePeriod(start_times_two[0], sample.duration * 2),  # adjacent must be joined together
        ]
    all_expected_periods = expected_periods_one + expected_periods_two
    log_list('Expected periods for server one', expected_periods_one)
    log_list('Expected periods for server two', expected_periods_two)

    for st in start_times_one:
        one.storage.save_media_sample(camera, st, sample)
    for st in start_times_two:
        two.storage.save_media_sample(camera, st, sample)
    one.rebuild_archive()
    two.rebuild_archive()
    assert all_expected_periods == one.get_recorded_time_periods(camera)
    assert all_expected_periods == two.get_recorded_time_periods(camera)
    return (one, two, expected_periods_one, expected_periods_two)


def test_separated_archive(server_factory, camera, sample_media_file):
    one, two, expected_periods_one, expected_periods_two = test_merged_archive(server_factory, camera, sample_media_file)
    one.change_system_id('{%s}' % uuid.uuid4())
    assert expected_periods_one == one.get_recorded_time_periods(camera)
    assert expected_periods_two == two.get_recorded_time_periods(camera)
