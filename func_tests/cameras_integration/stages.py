"""
This file contains all test stages, implementation rules are simple:

Exception is thrown = Failure, exception stack is returned.
Success result = Success is returned.
Failure result = Error is saved, retry will fallow.
Halt = Next iteration will fallow.
Stop iteration = Failure, last error is returned.
"""

import logging
from datetime import timedelta, datetime
import pytz
from typing import Generator, List

from framework.http_api import HttpError
from . import stage
from .checks import Checker, Failure, Halt, Result, Success, expect_values, retry_expect_values

from helper_functions import ffprobe, metadata, configure_video, configure_audio

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


@_stage(is_essential=True, timeout=timedelta(minutes=6))
def discovery(run, **kwargs):  # type: (stage.Run, dict) -> Generator[Result]
    if 'mac' not in kwargs:
        kwargs['mac'] = run.id

    if 'name' not in kwargs:
        kwargs['name'] = kwargs['model']

    while True:
        yield expect_values(kwargs, run.data)


@_stage(is_essential=True, timeout=timedelta(minutes=6))
def authorization(run, password, login=None):  # type: (stage.Run, str, str) -> Generator[Result]
    if password != 'auto':
        run.server.api.set_camera_credentials(run.data['id'], login, password)
        yield Halt('Try to set credentials')

    checker = Checker()
    while not checker.expect_values(dict(status="Online"), run.data):
        yield checker.result()

    yield Success()


@_stage()
def attributes(self, **kwargs):  # type: (stage.Run, dict) -> Generator[Result]
    while True:
        yield expect_values(kwargs, self.data)


@_stage(timeout=timedelta(minutes=6))
def recording(run, **configurations):  # type: (stage.Run, dict) -> Generator[Result]
    checker = Checker()
    # Checking if the camera is recording already
    while not checker.expect_values(dict(status="Online"), run.data):
        yield checker.result()

    with run.server.api.camera_recording(run.data['id']):
        yield Halt('Trying to start recording')

        while not checker.expect_values(dict(status="Recording"), run.data):
            yield checker.result()

        while True:
            recorded_periods = run.server.api.get_recorded_time_periods(run.data['id'])
            if not recorded_periods:
                yield Failure('No data is recorded')
                continue

            epoch = pytz.utc.localize(datetime.utcfromtimestamp(0))
            time_since_epoch = recorded_periods[-1].start - epoch

            for profile, configuration in configurations.items():
                archive_url = '{}?stream={}&pos={}'.format(
                    run.media_url,
                    {'primary': 0, 'secondary': 1}[profile],
                    int(time_since_epoch.total_seconds() * 1000))
                _logger.info('Archive request url for stream {}: {}'.format(
                    profile, archive_url))

                while True:
                    streams = ffprobe(archive_url)
                    if not streams:
                        continue
                    yield expect_values(configuration, metadata(streams[0]), '{}'.format(profile))


@_stage(timeout=timedelta(minutes=6))
def video_parameters(run, **configurations):  # type: (stage.Run, dict) -> Generator[Result]
    for profile, profile_configurations_list in configurations.items():
        stream_url = '{}?stream={}'.format(run.media_url, {'primary': 0, 'secondary': 1}[profile])
        for index, configuration in enumerate(profile_configurations_list):
            configure_video(
                run.server.api, run.id, run.data['cameraAdvancedParams'], profile, **configuration)
            while True:
                streams = ffprobe(stream_url)
                if not streams:
                    continue

                yield expect_values(
                    configuration, metadata(streams[0]), '{}[{}]'.format(profile, index))

    yield Success()


@_stage(timeout=timedelta(minutes=6))
def audio_parameters(run, *configurations):  # type: (stage.Run, dict) -> Generator[Result]
    """Enable audio on the camera; change the audio codec; check if the audio codec
    corresponds to the expected one. Disable the audio in the end.
    """
    with run.server.api.camera_audio_enabled(run.id):
        # Changing the audio codec accordingly to config
        for index, configuration in enumerate(configurations):
            if not configuration.get('skip_codec_change'):
                configure_audio(
                    run.server.api, run.id, run.data['cameraAdvancedParams'], **configuration)

            _logger.info('Check with ffprobe if the new audio codec corresponds to the config.')
            while True:
                try:
                    streams = ffprobe(run.media_url)
                    if not streams:
                        continue
                    audio_stream = streams[1]
                except IndexError:
                    yield Halt('Audio stream was not discovered by ffprobe.')
                    continue

                yield expect_values(
                    configuration, {'codec': audio_stream.get('codec_name').upper()}, index)


@_stage()
def io_events(run, ins, outs):
        # type: (stage.Run, list, list, bool) -> Generator[Result]
    expected_ports = {
        'id=' + port['id']: {'portType': type_, name: port.get('name', type_ + ' ' + port['id'])}
        for ports, type_, name in zip((ins, outs), ('Input', 'Output'), ('inputName', 'outputName'))
        for port in ports
    }

    checker = Checker()
    while not checker.expect_values(expected_ports, run.data['ioSettings'], 'io'):
        yield checker.result()

    camera_uuid = run.data['id']  # Event rules work with UUIDs only, physicalId will not work.
    run.server.api.make_event_rule(
        'cameraInputEvent', 'Active', 'diagnosticsAction', event_resource_ids=[camera_uuid])
    run.server.api.make_event_rule(
        'userDefinedEvent', 'Undefined', 'cameraOutputAction',
        event_condition_resource=run.id, action_resource_ids=[camera_uuid])

    run.server.api.create_event(source=run.id)
    for port in ins:
        if not port.get('connected'):
            continue

        while True:
            events = run.server.api.get_events(camera_uuid, 'cameraInputEvent')
            if events:
                yield expect_values({'eventParams.inputPortId': port['id']}, events[-1], 'event')
            else:
                yield Failure('No input events from camera')

    yield Success()


PTZ_CAPABILITY_FLAGS = {'presets': 0x10000, 'absolute': 0x40000070}


@_stage(timeout=timedelta(minutes=2))
def ptz_positions(run, *positions):  # type: (stage.Run, List[dict]) -> Generator[Result]
    for name, flag in PTZ_CAPABILITY_FLAGS.items():
        if run.data['ptzCapabilities'] & flag == 0:
            raise KeyError('PTZ {}({:b}) capabilities are not supported in {:b}'.format(
                name, flag, run.data['ptzCapabilities']))

    for use_preset in False, True:
        for position in positions:
            point = {k: int(v) for k, v in zip(
                ('pan', 'tilt', 'zoom'), position['point'].split('-'))}

            def execute(command, expected=None, **kwargs):
                return retry_expect_values(
                    expected, lambda: run.server.api.execute_ptz(run.id, command, **kwargs),
                    HttpError, '<{}>'.format(position['preset' if use_preset else 'point']))

            if use_preset:
                if 'preset' not in position:
                    continue

                name = position.get('name', position['preset'])
                for error in execute('GetPresets', {'id=' + position['preset']: {'name': name}}):
                    yield error

                for error in execute('ActivatePreset', speed=100, presetId=position['preset']):
                    yield error
            else:
                for error in execute('AbsoluteDeviceMove', speed=100, **point):
                    yield error

            yield Halt('Wait for move to {}'.format(point))
            for error in execute('GetDevicePosition'):
                yield error

    yield Success()
