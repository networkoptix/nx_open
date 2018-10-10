""""This file contains helper funcs used in stages.py"""

import json
import logging

from framework.os_access.exceptions import NonZeroExitStatus, Timeout
from framework.os_access.local_shell import local_shell

_logger = logging.getLogger(__name__)


def fps_avg(fps):
    try:
        fps_min, fps_max = fps
        fps_average = int((fps_min + fps_max) / 2)
    except (ValueError, TypeError):
        raise TypeError(
            'FPS should be a list of 2 ints e.g. [5, 10], however, config value is {}'.format(fps))
    return fps_average


def ffprobe_streams(stream_url):
    try:
        out = local_shell.run_sh_script(
            # language=Bash
            '''
                ffprobe -show_format -show_streams -analyzeduration 3M -probesize 2e+07 -of json "$URL"
                ''',
            env={'URL': stream_url},
            timeout_sec=30,
            )
    except (AssertionError, Timeout, NonZeroExitStatus) as error:
        _logger.debug("FFprobe error: %s", str(error))
        return

    try:
        result = json.loads(out.decode('utf-8'))
    except ValueError:
        _logger.debug('FFprobe returned bad JSON')
        return
    if not result:
        _logger.debug("FFprobe returned None")
        return
    return result.get('streams')


def ffprobe_video_metadata(stream):
    fps = int(stream['r_frame_rate'].split('/')[0]) / int(stream['r_frame_rate'].split('/')[1])
    # When ffprobe discovers a corrupted stream, it returns fps == 90000.
    # if fps >= 90000:
    #     _logger.debug('FFprobe returned fps = {}. No/corrupted stream?'.format(fps))
    #     return None
    return {
        'resolution': '{}x{}'.format(stream['width'], stream['height']),
        'codec': stream['codec_name'].upper(),
        'fps': fps,
    }


def ffprobe_audio_metadata(stream):
    return {'codec': stream.get('codec_name').upper()}


def ffprobe_metadata(stream_url):
    streams = ffprobe_streams(stream_url)
    if not streams:
        return None

    video, audio = None, None
    for stream in streams:
        if stream.get('codec_type') == 'video':
            video = ffprobe_video_metadata(stream)
        elif stream.get('codec_type') == 'audio':
            audio = ffprobe_audio_metadata(stream)

    return dict(video=video, audio=audio)


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


def configure_video(api, camera_id, camera_advanced_params, profile, fps=None, **configuration):
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
        new_cam_params[fps_param_id] = fps_avg(fps)

    api.set_camera_advanced_param(camera_id, **new_cam_params)


def configure_audio(api, camera_id, camera_advanced_params, codec):
    audio_input = _find_param_by_name_prefix(camera_advanced_params['groups'], 'root', 'audio')
    codec_param_id = _find_param_by_name_prefix(
        audio_input['params'], 'audio input', 'codec')['id']
    new_cam_params = dict()
    new_cam_params[codec_param_id] = codec
    api.set_camera_advanced_param(camera_id, **new_cam_params)
