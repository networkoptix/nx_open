""""This file contains helper funcs used in stages.py"""

import json
import logging
import subprocess
import traceback
from datetime import timedelta

from framework.waiting import Timer
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


def _expect_poll_output(title, expected_values, process, parse_output):
    timer = Timer()
    while process.poll() is None:
        if timer.from_start > timedelta(seconds=30):
            yield Halt('{!r} -- has timed out'.format(title))
            return
        yield Halt('{!r} -- is in progress'.format(title))

    stdout, stderr = process.communicate()
    _logger.debug('Process pid=%s -- stdout:\n%s', process.pid, stdout)
    _logger.debug('Process pid=%s -- stderr:\n%s', process.pid, stderr)
    if process.returncode != 0 and process.returncode != 124:  # 0 - success, 124 - timeout.
        yield Failure('{!r} -- returned error code: {}'.format(title, process.returncode))
        return

    try:
        actual_values = parse_output(stdout.decode('utf-8').strip()) if stdout else None
    except Exception as error:
        _logger.debug('Process pid=%s -- parsing error: %s', process.pid, traceback.format_exc())
        yield Failure('{!r} -- parsing error: {!s}'.format(title, error))
    else:
        yield expect_values(expected_values, actual_values, path=title)


def _expect_command_output(title, expected_values, parse_output, command, rerun_count=1000):
    last_failure = None
    for run_number in range(rerun_count):
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        _logger.debug('Run async pid=%s: %s', process.pid, ' '.join(command))
        try:
            retry = '-- retry {}'.format(run_number)
            for result in _expect_poll_output(title, expected_values, process, parse_output):
                if isinstance(result, Success):
                    return
                elif isinstance(result, Halt) and last_failure:
                    yield last_failure.with_more_errors([retry, result.message])
                elif isinstance(result, Failure):
                    last_failure = result
                    yield result.with_more_errors([retry], prefix=True)
                else:
                    yield result
        finally:
            try:
                process.kill()
            except OSError:
                pass


def _ffprobe_extract_video(output):
    stream = json.loads(output)['streams'][0]
    return dict(resolution='{width}x{height}'.format(**stream), codec=stream['codec_name'].upper())


def _ffprobe_extract_fps(output):
    if output.count('[') > output.count(']'):
        output += ']'
    if output.count('{') > output.count('}'):
        output += '}'
    frames = json.loads(output)['frames']
    return dict(fps=len(frames) / float(frames[-1]['pkt_pts_time']))


def _ffprobe_extract_audio(output):
    streams = json.loads(output)['streams'][0]
    return dict(codec=streams['codec_name'].upper())


def ffprobe_expect_stream(expected_values, stream_url, title):
    command = ['ffprobe', '-of', 'json', '-i', stream_url]
    video = expected_values.get('video')
    if video:
        fps = video.pop('fps') if 'fps' in video else None
        for result in _expect_command_output(
                title + '.ffprobe_v', video, _ffprobe_extract_video,
                command + ['-show_streams', '-select_streams', 'v', '-probesize', '10k']):
            yield result

        if fps:
            for result in _expect_command_output(
                    title + '.ffprobe_f', {'fps': fps}, _ffprobe_extract_fps,
                    ['timeout', '10s'] + command + ['-show_frames', '-select_streams', 'v']):
                yield result

    audio = expected_values.get('audio')
    if audio:
        for result in _expect_command_output(
                title + '.ffprobe_a', audio, _ffprobe_extract_audio,
                command + ['-show_streams', '-select_streams', 'a', '-probesize', '10k']):
            yield result


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
    audio_group = _find_param_by_name_prefix(
        camera_advanced_params['groups'], 'root', 'audio')
    try:
        codec_param = _find_param_by_name_prefix(audio_group['params'], 'group', 'codec')
    except KeyError:
        sub_group = _find_param_by_name_prefix(audio_group['groups'], 'group', 'input settings')
        codec_param = _find_param_by_name_prefix(sub_group['params'], 'sub group', 'audio encoding')

    new_cam_params = dict()
    new_cam_params[codec_param['id']] = codec
    api.set_camera_advanced_param(camera_id, **new_cam_params)
