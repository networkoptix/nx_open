"""
This file contains all test stages, implementation rules are simple:

Exception is thrown = Failure, exception stack is returned.
Success result = Success is returned.
Failure result = Error is saved, retry will fallow.
Halt = Next iteration will fallow.
Stop iteration = Failure, last error is returned.
"""

import logging
from datetime import timedelta
from typing import Generator, List

import ffmpeg
from framework.camera import Camera

from . import stage
from .checks import Checker, Failure, Halt, Result, Success, expect_values

# Filled by _stage decorator.
LIST = []  # type: List[stage.Stage]

_logger = logging.getLogger(__name__)

def _stage(is_essential=False, timeout=timedelta(seconds=30)):
    """:param is_essential - if True and stage is failed then no other stages will be executed.
    """
    def decorator(actions):
        new_stage = stage.Stage(actions.__name__, actions, is_essential, timeout)
        LIST.append(new_stage)
        return new_stage

    return decorator


@_stage(is_essential=True, timeout=timedelta(minutes=2))
def discovery(run, **kwargs):  # type: (stage.Run) -> Generator[Result]
    if 'mac' not in kwargs:
        kwargs['mac'] = run.id

    if 'name' not in kwargs:
        kwargs['name'] = kwargs['model']

    while True:
        yield expect_values(kwargs, run.data)


@_stage(is_essential=True, timeout=timedelta(minutes=2))
def authorization(run, password, login=None):  # type: (stage.Run, str, str) -> Generator[Result]
    if password != 'auto':
        run.server.api.set_camera_credentials(run.data['id'], login, password)
        yield Halt('Try to set credentials')

    checker = Checker()
    while not checker.expect_values(dict(status="Online"), run.data):
        yield checker.result()

    yield Success()


@_stage()
def attributes(self, **kwargs):  # type: (stage.Run) -> Generator[Result]
    while True:
        yield expect_values(kwargs, self.data)


@_stage(timeout=timedelta(minutes=6))
def recording(run, fps=30, **streams):  # type: (stage.Run) -> Generator[Result]
    with run.server.api.camera_recording(run.data['id'], fps=fps):
        yield Halt('Try to start recording')

        checker = Checker()
        while not checker.expect_values(dict(status="Recording"), run.data):
            yield checker.result()

        if not run.server.api.get_recorded_time_periods(run.data['id']):
            # TODO: Verify recorded periods and try to pull video data.
            yield Failure('No data is recorded')

    def stream_field_and_key(key):
        return {
            'fps': ('bitrateInfos', 'actualFps'),
            'codec': ('mediaStreams', key),
            'resolution': ('mediaStreams', key),
        }[key]

    expected_streams = {}
    for stream, values in streams.items():
        for key, value in values.items():
            field_name, field_key = stream_field_and_key(key)
            field = expected_streams.setdefault(field_name + '.streams', {})
            field.setdefault('encoderIndex=' + stream, {})[field_key] = value

    while not checker.expect_values(expected_streams, run.data):
        yield checker.result()

    yield Success()


@_stage(timeout=timedelta(seconds=15))
def stream_parameters(run, **kwargs):
    while True:
        url = run.server.api.generic.http.url(run.id, media=True, with_auth=True)
        stream = ffmpeg.probe(url)['streams'][0]
        metadata = dict(
            codec=stream['codec_name'],
            resolution='{}x{}'.format(stream['coded_width'], stream['coded_height']),
            fps=int(stream['r_frame_rate'].split('/')[0]))

        yield expect_values(kwargs, metadata, 'stream params')