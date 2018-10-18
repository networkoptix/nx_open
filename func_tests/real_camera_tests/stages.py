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
from .camera_actions import configure_audio, configure_video, ffprobe_streams, fps_avg
from .checks import Checker, Failure, Halt, Result, Success, expect_values, retry_expect_values

# Filled by _stage decorator.
LIST = []  # type: List[stage.Stage]

_logger = logging.getLogger(__name__)


def _stage(is_essential=False, timeout=timedelta(seconds=30)):
    """Registers stage for execution:
    :param is_essential - if True and stage is failed then no other stages will be executed.
    :param timeout - specifies how long next iterations will fallow before failure.
    """

    def decorator(actions):
        new_stage = stage.Stage(actions.__name__, actions, is_essential, timeout)
        LIST.append(new_stage)
        return new_stage

    return decorator


@_stage(is_essential=True, timeout=timedelta(minutes=3))
def discovery(run, mac=None, name=None, **kwargs):  # type: (stage.Run, dict) -> Generator[Result]
    """Checks if camera has been discovered by server with specified attributes.
    """
    kwargs['mac'] = mac or run.id
    kwargs['name'] = name or kwargs['model']

    while True:
        yield expect_values(kwargs, run.data)


@_stage(is_essential=True, timeout=timedelta(minutes=2))
def authorization(run, password, login=None):  # type: (stage.Run, str, str) -> Generator[Result]
    """Checks if camera authorizes with provided credentials.
    If password='auto', server is supposed to autodetect login and password.
    """
    if password != 'auto':
        run.server.api.set_camera_credentials(run.uuid, login, password)
        yield Halt('Try to set credentials')

    checker = Checker()
    while not checker.expect_values(dict(status="Online"), run.data):
        yield checker.result()

    yield Success()


@_stage()
def attributes(self, **kwargs):  # type: (stage.Run, dict) -> Generator[Result]
    """Checks if camera has specified attributes.
    """
    while True:
        yield expect_values(kwargs, self.data)


@_stage(timeout=timedelta(minutes=7))
def recording(run, primary, secondary=None):  # type: (stage.Run, dict, dict) -> Generator[Result]
    """For echo FPS in primary config enables recording; checks if primary and secondary stream
    parameters match to config values.
    """
    for fps_index, fps_range in enumerate(primary['fps']):
        selected = primary.copy()
        selected['fps'] = fps_range
        with run.server.api.camera_recording(run.uuid, fps=fps_avg(fps_range)):
            for error in retry_expect_values(dict(status="Recording"), lambda: run.data):
                yield error

            while not run.server.api.get_recorded_time_periods(run.uuid):
                yield Failure('No data is recorded')

            for profile, configuration in (('primary', selected), ('secondary', secondary)):
                if configuration:
                    for error in ffprobe_streams(
                            {'video': configuration}, run.media_url(profile),
                            '{}[{}]'.format(profile, fps_index)):
                        yield error

    yield Success()


@_stage(timeout=timedelta(minutes=7))
def video_parameters(run, stream_urls=None, **profiles):
        # type: (stage.Run, dict, dict) -> Generator[Result]
    """For each stream and it's configuration: enables recording; applies configuration and checks
    if actual stream parameters correspond to it.
    """
    # Enable recording to keep video stream open during entire stage.
    with run.server.api.camera_recording(run.uuid):
        for profile, configurations in profiles.items():
            for index, configuration in enumerate(configurations):
                configure_video(
                    run.server.api, run.id, run.data['cameraAdvancedParams'], profile, **configuration)

                for error in ffprobe_streams(
                        {'video': configuration}, run.media_url(profile),
                        '{}[{}]'.format(profile, index)):
                    yield error

    if stream_urls:
        for error in retry_expect_values({'streamUrls': stream_urls}, lambda: run.data, syntax='*'):
            yield error
    yield Success()


@_stage(timeout=timedelta(minutes=1))
def audio_parameters(run, *configurations):  # type: (stage.Run, dict) -> Generator[Result]
    """For each configuration: enables recording with audio; applies configuration and checks if
    actual stream parameters on primary stream correspond to it.
    """
    # Enable recording to keep video stream open during entire stage.
    with run.server.api.camera_recording(run.uuid):
        with run.server.api.camera_audio_enabled(run.uuid):
            for index, configuration in enumerate(configurations):
                if not configuration.get('skip_codec_change'):
                    configure_audio(
                        run.server.api, run.id, run.data['cameraAdvancedParams'], **configuration)
                else:
                    del configuration["skip_codec_change"]

                for error in ffprobe_streams(
                        {'audio': configuration}, run.media_url(), 'primary[{}]'.format(index)):
                    yield error

    yield Success()


@_stage()
def io_events(run, ins, outs):
        # type: (stage.Run, list, list, bool) -> Generator[Result]
    """Checks if camera has specified input and output ports.
    If some inputs have connected outputs: creates event rule on input; creates generic event to
    trigger output; generates generic event and checks if input event rule is triggered.
    """
    expected_ports = {
        'id=' + port['id']: {'portType': type_, name: port.get('name', type_ + ' ' + port['id'])}
        for ports, type_, name in zip((ins, outs), ('Input', 'Output'), ('inputName', 'outputName'))
        for port in ports
    }

    checker = Checker()
    while not checker.expect_values(expected_ports, run.data['ioSettings'], 'io'):
        yield checker.result()

    run.server.api.make_event_rule(
        'cameraInputEvent', 'Active', 'diagnosticsAction', event_resource_ids=[run.uuid])
    run.server.api.make_event_rule(
        'userDefinedEvent', 'Undefined', 'cameraOutputAction',
        event_condition_resource=run.id, action_resource_ids=[run.uuid])

    run.server.api.create_event(source=run.id)
    for port in ins:
        if not port.get('connected'):
            continue

        while True:
            events = run.server.api.get_events(run.uuid, 'cameraInputEvent')
            if events:
                yield expect_values({'eventParams.inputPortId': port['id']}, events[-1], 'event')
            else:
                yield Failure('No input events from camera')

    yield Success()


PTZ_CAPABILITY_FLAGS = {'presets': 0x10000, 'absolute': 0x40000070}


@_stage()
def ptz_positions(run, *positions):  # type: (stage.Run, List[dict]) -> Generator[Result]
    """For each position: checks if camera can be moved to this position. If positions have attached
    presets: checks if server imported such preset and can move camera to it.
    """
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

            yield Halt('Wait for move to {} by {}'.format(
                point, 'preset' if use_preset else 'point'))
            for error in execute('GetDevicePosition'):
                yield error

    yield Success()
