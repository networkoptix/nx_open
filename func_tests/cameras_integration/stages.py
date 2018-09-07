"""
This file contains all test stages, implementation rules are simple:

Exception is thrown = Failure, exception stack is returned.
Success result = Success is returned.
Failure result = Error is saved, retry will fallow.
Halt = Next iteration will fallow.
Stop iteration = Failure, last error is returned.
"""

import json
import logging
from datetime import timedelta, datetime
import pytz

import subprocess32 as subprocess
from typing import Generator, List

from framework.http_api import HttpError
from . import stage
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


def _fps_avg(fps):
    try:
        fps_min, fps_max = fps
        fps_average = int((fps_min + fps_max) / 2)
    except (ValueError, TypeError):
        raise TypeError(
            'FPS should be a list of 2 ints e.g. [5, 10], however, config value is {}'.format(fps))
    return fps_average


class FFProbeError(Exception):
    pass


def _ffprobe(stream_url):
    args = ['ffprobe', '-show_format', '-show_streams', '-of', 'json', stream_url]
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out, err = process.communicate(timeout=15)
        if process.returncode != 0:
            raise FFProbeError("FFprobe returned non-zero return code")
        return json.loads(out.decode('utf-8'))
    except subprocess.TimeoutExpired:
        _logger.info('FFprobe timeout reached. Killing FFprobe')
        process.kill()


def _metadata(stream):
    fps = int(stream['r_frame_rate'].split('/')[0]) / int(stream['r_frame_rate'].split('/')[1])
    metadata = {
        'resolution': '{}x{}'.format(stream['width'], stream['height']),
        'codec': stream['codec_name'].upper(),
        'fps': fps,
        }
    return metadata


@_stage(timeout=timedelta(minutes=2))
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
                _logger.info('Archive request url for stream {}:{}'.format(
                    profile, archive_url))

                try:
                    streams = _ffprobe(archive_url)
                    if not streams:
                        yield Failure('ffprobe returned None, trying again')
                        continue
                    stream = streams['streams'][0]
                    _logger.info(
                        "FFprobe output for videostream, profile {}:\n{}"
                        .format(profile, stream)
                        )
                except FFProbeError:
                    yield Failure('ffprobe error: trying to run ffprobe again')
                    continue

                yield expect_values(configuration, _metadata(stream), '{}'.format(profile))



def _find_param_by_name_prefix(all_params, parent_group, *name_prefixes):
    """Searching by prefix is used due to different names for primary stream group in the settings
    of different cameras (vendor dependent), but they all start with "Primary". At the moment only
    2 possibilities exist: "Primary Stream" for Hanwha and "Primary" for others. The same logic
    applies to the secondary stream group name.
    """
    for param in all_params:
        name = param['name'].lower()
        for prefix in name_prefixes:
            if prefix in name:
                return param
    raise KeyError('No {} in advanced params {}'.format(repr(name_prefixes), parent_group))


def _configure_video(api, camera_id, camera_advanced_params, profile, fps=None, **configuration):
    # Setting up the advanced parameters
    streaming = _find_param_by_name_prefix(
        camera_advanced_params['groups'], 'root', 'streaming', 'streams')
    stream = _find_param_by_name_prefix(streaming['groups'], 'streaming', profile)
    codec_param_id = _find_param_by_name_prefix(stream['params'], profile, 'codec')['id']
    resolution_param_id = _find_param_by_name_prefix(stream['params'], profile, 'resolution')['id']

    new_cam_params = {}
    new_cam_params[codec_param_id] = configuration['codec']
    new_cam_params[resolution_param_id] = configuration['resolution']
    # FPS configuration is stored in the configuration file and is specified by range
    # (list of 2 values). We take the average fps value from this range:
    if fps:
        fps_param_id = _find_param_by_name_prefix(
            stream['params'], profile, 'fps', 'frame rate')['id']
        new_cam_params[fps_param_id] = _fps_avg(fps)

    api.set_camera_advanced_param(camera_id, **new_cam_params)


@_stage(timeout=timedelta(minutes=6))
def stream_parameters(run, **configurations):  # type: (stage.Run, dict) -> Generator[Result]
    for profile, profile_configurations_list in configurations.items():
        stream_url = '{}?stream={}'.format(run.media_url, {'primary': 0, 'secondary': 1}[profile])
        for index, configuration in enumerate(profile_configurations_list):
            _configure_video(
                run.server.api, run.id, run.data['cameraAdvancedParams'], profile, **configuration)
            while True:
                try:
                    streams = _ffprobe(stream_url).get('streams')
                    if not streams:
                        yield Failure('No video stream was found by ffprobe, trying again')
                        continue
                    stream = streams[0]
                except FFProbeError:
                    _logger.info('ffprobe error: trying to run ffprobe again')
                    continue

                yield expect_values(
                    configuration, _metadata(stream), '{}[{}]'.format(profile, index))

    yield Success()


def _configure_audio(api, camera_id, camera_advanced_params, codec):
    audio_input = _find_param_by_name_prefix(camera_advanced_params['groups'], 'root', 'audio')
    codec_param_id = _find_param_by_name_prefix(
        audio_input['params'], 'audio input', 'codec')['id']
    new_cam_params = dict()
    new_cam_params[codec_param_id] = codec
    api.set_camera_advanced_param(camera_id, **new_cam_params)


@_stage(timeout=timedelta(minutes=6))
def audio(run, *configurations):  # type: (stage.Run, dict) -> Generator[Result]
    """Enable audio on the camera; changes the audio codec; check if the audio codec
    corresponds to the expected one. Disable the audio in the end.
    """
    with run.server.api.camera_audio_enabled(run.id):
        # Changing the audio codec accordingly to config
        for index, configuration in enumerate(configurations):
            if not configuration.get('skip_codec_change'):
                _configure_audio(
                    run.server.api, run.id, run.data['cameraAdvancedParams'], **configuration)

            _logger.info('Check with ffprobe if the new audio codec corresponds to the config.')
            while True:
                try:
                    audio_stream = _ffprobe(run.media_url)['streams'][1]
                    _logger.info('Audio stream params retrieved by ffprobe: {}'
                                 .format(audio_stream))
                except FFProbeError:
                    yield Halt('ffprobe error: trying to run ffprobe again')
                    continue
                except IndexError:
                    yield Halt('Audio stream was not discovered by ffprobe.')
                    continue

                result = expect_values(configuration, {'codec': audio_stream.get('codec_name')
                                       .upper()}, index)

                yield result


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
