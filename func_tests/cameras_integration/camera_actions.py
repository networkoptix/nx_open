""""This file contains helper funcs used in stages.py"""

import os
import logging
import json
import subprocess

from framework.os_access.local_shell import local_shell
from framework.os_access.exceptions import Timeout, NonZeroExitStatus

_logger = logging.getLogger(__name__)


def _fps_avg(fps):
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
                ffprobe -show_format -show_streams -of json "$URL" &
                pid=$!
                for i in `seq 1 10`; do
                    if ps aux | awk '{print $2}' | grep $pid >/dev/null 2>&1; then
                        sleep 1
                    else
                        exit 0
                    fi
                done
                kill -9 $pid
                exit 1
                ''',
            env={'URL': stream_url})
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


def ffprobe_metadata(stream):
    fps = int(stream['r_frame_rate'].split('/')[0]) / int(stream['r_frame_rate'].split('/')[1])
    metadata = {
        'resolution': '{}x{}'.format(stream['width'], stream['height']),
        'codec': stream['codec_name'].upper(),
        'fps': fps,
        }
    return metadata


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
        new_cam_params[fps_param_id] = _fps_avg(fps)

    api.set_camera_advanced_param(camera_id, **new_cam_params)


def configure_audio(api, camera_id, camera_advanced_params, codec):
    audio_input = _find_param_by_name_prefix(camera_advanced_params['groups'], 'root', 'audio')
    codec_param_id = _find_param_by_name_prefix(
        audio_input['params'], 'audio input', 'codec')['id']
    new_cam_params = dict()
    new_cam_params[codec_param_id] = codec
    api.set_camera_advanced_param(camera_id, **new_cam_params)
