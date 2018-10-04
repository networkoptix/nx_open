"""
This file contains all test stages, implementation rules are simple:

Exception is thrown = Failure, exception stack is returned.
Success result = Success is returned.
Failure result = Error is saved, retry will follow.
Halt = Next iteration will follow.
Stop iteration = Failure, last error is returned.
"""

import logging
import timeit
from datetime import datetime, timedelta

from typing import Generator, List

from framework.http_api import HttpError
from . import stage
from .camera_actions import configure_audio, configure_video, ffprobe_metadata, ffprobe_streams, \
    fps_avg
from .checks import Checker, Failure, Halt, Result, Success, expect_values, retry_expect_values

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
def recording(run, primary, secondary=None):  # type: (stage.Run, dict, dict) -> Generator[Result]
    for fps_index, fps_range in enumerate(primary['fps']):
        # Make sure there is a gap between recordings.
        for error in retry_expect_values(dict(status="Online"), lambda: run.data):
            yield error

        current_configuration = primary.copy()
        current_configuration['fps'] = fps_range
        with run.server.api.camera_recording(run.data['id'], fps=fps_avg(fps_range)):
            for error in retry_expect_values(dict(status="Recording"), lambda: run.data):
                yield error

            while True:
                recorded_periods = run.server.api.get_recorded_time_periods(run.data['id'])
                if recorded_periods:
                    break
                yield Failure('No data is recorded')

            for profile, configuration in (
                    ('primary', current_configuration), ('secondary', secondary)):
                if configuration:
                    for error in retry_expect_values(
                            {'video': configuration},
                            lambda: ffprobe_metadata(run.media_url(profile)),
                            path=profile):
                        yield error

    yield Success()


@_stage(timeout=timedelta(minutes=6))
def video_parameters(run, stream_urls=None, **profiles
                     ):  # type: (stage.Run, dict, dict) -> Generator[Result]
    for profile, configurations in profiles.items():
        for index, configuration in enumerate(configurations):
            configure_video(
                run.server.api, run.id, run.data['cameraAdvancedParams'], profile, **configuration)
            for error in retry_expect_values(
                    {'video': configuration},
                    lambda: ffprobe_metadata(run.media_url(profile)),
                    path='{}{}'.format(profile, index)):
                yield error

    for error in retry_expect_values({'streamUrls': stream_urls}, lambda: run.data, syntax='*'):
        yield error

    yield Success()


@_stage(timeout=timedelta(minutes=6))
def audio_parameters(run, *configurations):  # type: (stage.Run, dict) -> Generator[Result]
    """Enable audio on the camera; change the audio codec; check if the audio codec
    corresponds to the expected one. Disable the audio in the end.
    """
    with run.server.api.camera_audio_enabled(run.data['id']):
        for index, configuration in enumerate(configurations):
            if not configuration.get('skip_codec_change'):
                configure_audio(
                    run.server.api, run.id, run.data['cameraAdvancedParams'], **configuration)
            else:
                del configuration["skip_codec_change"]

            for error in retry_expect_values(
                    {'audio': configuration},
                    lambda: ffprobe_metadata(run.media_url()),
                    path=index):
                yield error

    yield Success()


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
