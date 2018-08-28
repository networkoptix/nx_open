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

import ffmpeg
from typing import Generator, List

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


@_stage(is_essential=True, timeout=timedelta(minutes=3))
def discovery(run, **kwargs):  # type: (stage.Run, dict) -> Generator[Result]
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
def attributes(self, **kwargs):  # type: (stage.Run, dict) -> Generator[Result]
    while True:
        yield expect_values(kwargs, self.data)


@_stage(timeout=timedelta(minutes=6))
def recording(run, fps=30, **streams):  # type: (stage.Run, int, dict) -> Generator[Result]

    def stream_field_and_key(key):
        return {
            'fps': ('bitrateInfos', 'actualFps'),
            'codec': ('mediaStreams', key),
            'resolution': ('mediaStreams', key),
            }[key]

    with run.server.api.camera_recording(run.data['id'], fps=fps):
        yield Halt('Try to start recording')

        checker = Checker()
        while not checker.expect_values(dict(status="Recording"), run.data):
            yield checker.result()

        if not run.server.api.get_recorded_time_periods(run.data['id']):
            # TODO: Verify recorded periods and try to pull video data.
            yield Failure('No data is recorded')

        expected_streams = {}
        for stream, values in streams.items():
            for key, value in values.items():
                field_name, field_key = stream_field_and_key(key)
                field = expected_streams.setdefault(field_name + '.streams', {})
                field.setdefault('encoderIndex=' + stream, {})[field_key] = value

        while not checker.expect_values(expected_streams, run.data):
            yield checker.result()

        yield Success()


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
    streaming = _find_param_by_name_prefix(camera_advanced_params['groups'], 'root', 'streaming', 'streams')
    stream = _find_param_by_name_prefix(streaming['groups'], 'streaming', profile)
    codec_param_id = _find_param_by_name_prefix(stream['params'], profile, 'codec')['id']
    resolution_param_id = _find_param_by_name_prefix(stream['params'], profile, 'resolution')['id']
    if fps:
        fps_param_id = _find_param_by_name_prefix(stream['params'], profile, 'fps', 'frame rate')['id']

    new_cam_params = {}
    new_cam_params[codec_param_id] = configuration['codec']
    new_cam_params[resolution_param_id] = configuration['resolution']
    # fps configuration is stored in the configuration file and is specified by range
    # (list of 2 values). We take the average fps value from this range:
    if fps:
        try:
            fps_min, fps_max = fps
            fps_average = int((fps_min + fps_max) / 2)
        except (ValueError, TypeError):
            raise TypeError('FPS should be a list of 2 ints e.g. [15, 20], however, config value is {}'.format(fps))

        new_cam_params[fps_param_id] = fps_average

    api.set_camera_advanced_param(camera_id, **new_cam_params)


@_stage(timeout=timedelta(minutes=2))
def stream_parameters(run, **configurations):  # type: (stage.Run, dict) -> Generator[Result]
    for profile, profile_configurations_list in configurations.items():
        stream_url = '{}?stream={}'.format(run.media_url, {'primary': 0, 'secondary': 1}[profile])
        for index, configuration in enumerate(profile_configurations_list):
            _configure_video(run.server.api, run.id, run.data['cameraAdvancedParams'], profile, **configuration)
            while True:
                try:
                    stream = ffmpeg.probe(stream_url)['streams'][0]
                except ffmpeg.Error:
                    _logger.info('ffprobe error: trying to run ffprobe again')
                    continue

                metadata = {
                    'resolution': '{}x{}'.format(stream['width'], stream['height']),
                    'codec': stream['codec_name'].upper(),
                    'fps': int(stream['r_frame_rate'].split('/')[0]),
                    }

                result = expect_values(configuration, metadata, '{}[{}]'.format(profile, index))
                if isinstance(result, Success):
                    break

                yield result

    yield Success()


def _configure_audio(api, camera_id, camera_advanced_params, codec):
    audio_input = _find_param_by_name_prefix(camera_advanced_params['groups'], 'root', 'audio')
    codec_param_id = _find_param_by_name_prefix(audio_input['params'], 'audio input', 'codec')['id']
    new_cam_params = dict()
    new_cam_params[codec_param_id] = codec
    api.set_camera_advanced_param(camera_id, **new_cam_params)


@_stage(timeout=timedelta(minutes=2))
def audio(run, *configurations):  # type: (stage.Run, dict) -> Generator[Result]
    """Enables audio on the camera; changes the audio codec; checks if the audio codec
    corresponds to the expected one. Disables the audio in the end.
    """
    with run.server.api.camera_audio(run.id):
        # Changing the audio codec accordingly to config
        for index, configuration in enumerate(configurations):
            if 'skip_codec_change' in configuration.keys():
                continue
            else:
                _configure_audio(run.server.api, run.id, run.data['cameraAdvancedParams'], **configuration)

            # Checking with ffprobe if the new audio codec corresponds to the config
            while True:
                try:
                    audio_stream = ffmpeg.probe(run.media_url)['streams'][1]
                    _logger.info('Audio stream params retrieved by ffprobe: {}'.format(audio_stream))
                except ffmpeg.Error as error:
                    yield Halt('ffprobe error: {}'.format(error))
                    continue
                except IndexError:
                    yield Halt('Audio stream was not discovered by ffprobe')
                    continue

                result = expect_values(configuration, {'codec': audio_stream.get('codec_name').upper()}, index)
                if isinstance(result, Success):
                    break
                yield result

        yield Success()
