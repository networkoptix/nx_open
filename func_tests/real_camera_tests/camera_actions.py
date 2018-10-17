""""This file contains helper funcs used in stages.py"""

import json
import logging
import subprocess

import timeit

from .checks import Success, Halt, Failure, expect_values

_logger = logging.getLogger(__name__)


def fps_avg(fps):
    try:
        fps_min, fps_max = fps
        fps_average = int((fps_min + fps_max) / 2)
    except (ValueError, TypeError):
        raise TypeError(
            'FPS should be a list of 2 ints e.g. [5, 10], however, config value is {}'.format(fps))
    return fps_average


def _ffprobe_poll(expected_values, probe, stream_url, title):
    start_time = timeit.default_timer()
    while probe.poll() is None:
        if timeit.default_timer() - start_time > 30:
            yield Halt('{!r} ffprobe has timed out'.format(title))
            return
        yield Halt('{!r} ffprobe is in progress'.format(title))

    stdout, stderr = probe.communicate()
    if stdout:
        _logger.debug('FFprobe(%s) stdout:\n%s', stream_url, stdout)
    if stderr:
        _logger.debug('FFprobe(%s) stderr:\n%s', stream_url, stderr)
    if probe.returncode != 0:
        yield Halt('{!r} ffprobe returned error code {}'.format(title, probe.returncode))
        return

    streams = (json.loads(stdout.decode('utf-8')) or {}).get('streams')
    if not streams:
        yield Halt('{!r} ffprobe returned no streams'.format(title))
        return

    video, audio = None, None
    for stream in streams:
        if stream.get('codec_type') == 'video':
            fps_count, fps_base = stream['r_frame_rate'].split('/')
            video = {
                'resolution': '{}x{}'.format(stream['width'], stream['height']),
                'codec': stream['codec_name'].upper(),
                'fps': float(fps_count) / float(fps_base),
            }
        elif stream.get('codec_type') == 'audio':
            audio = {'codec': stream.get('codec_name').upper()}

    yield expect_values(expected_values, dict(video=video, audio=audio), path=title)


def ffprobe_streams(expected_values, stream_url, title, rerun_count=1000):
    frames = max(expected_values.get('video', {}).get('fps', [30]))
    last_failure = None
    for run_number in range(rerun_count):
        options = ['-show_streams', '-of', 'json', '-fpsprobesize', str(frames)]
        probe = subprocess.Popen(
            ['ffprobe'] + options + [stream_url],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            for result in _ffprobe_poll(expected_values, probe, stream_url, title):
                if isinstance(result, Success):
                    return
                elif isinstance(result, Halt) and last_failure:
                    yield last_failure.with_more_errors(
                        'retry {}:'.format(run_number), result.message)
                elif isinstance(result, Failure):
                    last_failure = result
                    yield result
                else:
                    yield result
        finally:
            try:
                probe.kill()
            except OSError:
                pass


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
