#!/usr/bin/env python3
import itertools
import logging
import math
import platform
import re
import signal
import sys
import time
import uuid
from typing import List, Tuple, Optional

from vms_benchmark.camera import Camera

# This block ensures that ^C interrupts are handled quietly.
from vms_benchmark.exceptions import VmsBenchmarkError, VmsBenchmarkIssue

try:
    def exit_handler(signum, _frame):
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        sys.exit(128 + signum)

    signal.signal(signal.SIGINT, exit_handler)
    signal.signal(signal.SIGTERM, exit_handler)
    if platform.system() == 'Linux':
        # Prevent "[Errno 32] Broken pipe" exceptions when writing to a pipe.
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)

except KeyboardInterrupt:
    sys.exit(128 + signal.SIGINT)

if platform.system() == 'Linux':
    def debug_signal(_signum, _frame):
        import pdb
        pdb.set_trace()

    if platform.python_implementation() == 'Jython':
        debug_signum = signal.SIGUSR2  # bug #424259
    else:
        debug_signum = signal.SIGUSR1

    signal.signal(debug_signum, debug_signal)

from os import path as osp

project_root = osp.dirname(osp.dirname(osp.realpath(__file__)))
if osp.isfile(osp.join(project_root, ".not_installed")):
    sys.path.insert(0, osp.join(project_root, "lib"))

from vms_benchmark.config import ConfigParser
from vms_benchmark.box_connection import BoxConnection
from vms_benchmark.box_platform import BoxPlatform
from vms_benchmark.vms_scanner import VmsScanner
from vms_benchmark.server_api import ServerApi
from vms_benchmark import test_camera_runner, vms_scanner, box_connection, box_platform
from vms_benchmark import stream_reader_runner
from vms_benchmark.linux_distibution import LinuxDistributionDetector
from vms_benchmark import box_tests
from vms_benchmark import host_tests
from vms_benchmark import exceptions

import urllib
import urllib.error
import click
import threading


conf_definition = {
    "boxHostnameOrIp": {"type": "str"},
    "boxLogin": {"type": "str", "default": ""},
    "boxPassword": {"type": "str", "default": ""},
    "boxSshPort": {"type": "int", "range": [1, 65535], "default": 22},
    "vmsUser": {"type": "str"},
    "vmsPassword": {"type": "str"},
    "virtualCameraCount": {"type": "intList", "range": [1, 999]},
    "liveStreamsPerCameraRatio": {"type": "float", "range": [0.0, 999.0], "default": 1.0},
    "archiveStreamsPerCameraRatio": {"type": "float", "range": [0.0, 999.0], "default": 0.2},
    "streamingTestDurationMinutes": {"type": "int", "range": [1, None], "default": 4 * 60},
    "cameraDiscoveryTimeoutSeconds": {"type": "int", "range": [0, None], "default": 3 * 60},
    "archiveDeletingTimeoutSeconds": {"type": "int", "range": [0, None], "default": 60},
}


ini_definition = {
    "testcameraBin": {"type": "str", "default": "./testcamera/testcamera"},
    "rtspPerfBin": {"type": "str", "default": "./testcamera/rtsp_perf"},
    "testFileHighResolution": {"type": "str", "default": "./high.ts"},
    "testFileHighDurationMs": {"type": "int", "range": [1, None], "default": 10000},
    "testFileLowResolution": {"type": "str", "default": "./low.ts"},
    "testStreamFpsHigh": {"type": "int", "range": [1, 999], "default": 30},
    "testStreamFpsLow": {"type": "int", "range": [1, 999], "default": 7},
    "testcameraOutputFile": {"type": "str", "default": ""},
    "testcameraLocalInterface": {"type": "str", "default": ""},
    "cpuUsageThreshold": {"type": "float", "range": [0.0, 1.0], "default": 0.5},
    "archiveBitratePerCameraMbps": {"type": "int", "range": [1, 999], "default": 10},
    "minimumArchiveFreeSpacePerCameraSeconds": {"type": "int", "range": [1, None], "default": 240},
    "timeDiffThresholdSeconds": {"type": "float", "range": [0.0, None], "default": 180},
    "swapThresholdKilobytes": {"type": "int", "range": [0, None], "default": 0},
    "sleepBeforeCheckingArchiveSeconds": {"type": "int", "range": [0, None], "default": 100},
    "maxAllowedFrameDrops": {"type": "int", "range": [0, None], "default": 0},
    "ramPerCameraMegabytes": {"type": "int", "range": [1, None], "default": 40},
    "sshCommandTimeoutS": {"type": "int", "range": [1, None], "default": 5},
    "sshServiceCommandTimeoutS": {"type": "int", "range": [1, None], "default": 30},
    "sshGetFileContentTimeoutS": {"type": "int", "range": [1, None], "default": 30},
    "sshGetProcMeminfoTimeoutS": {"type": "int", "range": [1, None], "default": 10},
    "rtspPerfLinesOutputFile": {"type": "str", "default": ""},
    "rtspPerfStderrFile": {"type": "str", "default": ""},
    "archiveReadingPosS": {"type": "int", "range": [0, None], "default": 15},
    "apiReadinessTimeoutSeconds": {"type": "int", "range": [1, None], "default": 30},
    "worstAllowedStreamLagUs": {"type": "int", "range": [0, None], "default": 5_000_000},
    "maxAllowedPtsDiffDeviationUs": {"type": "int", "range": [0, None], "default": 2000},
}


def load_configs(conf_file, ini_file):
    conf = ConfigParser(conf_file, conf_definition)
    ini = ConfigParser(ini_file, ini_definition, is_file_optional=True)

    test_camera_runner.ini_testcamera_bin = ini['testcameraBin']
    test_camera_runner.ini_test_file_high_resolution = ini['testFileHighResolution']
    test_camera_runner.ini_test_file_low_resolution = ini['testFileLowResolution']
    test_camera_runner.ini_testcamera_output_file = ini['testcameraOutputFile']
    stream_reader_runner.ini_rtsp_perf_bin = ini['rtspPerfBin']
    stream_reader_runner.ini_rtsp_perf_stderr_file = ini['rtspPerfStderrFile']
    box_connection.ini_ssh_command_timeout_s = ini['sshCommandTimeoutS']
    box_connection.ini_ssh_get_file_content_timeout_s = ini['sshGetFileContentTimeoutS']
    vms_scanner.ini_ssh_service_command_timeout_s = ini['sshServiceCommandTimeoutS']
    box_platform.ini_ssh_get_proc_meminfo_timeout_s = ini['sshGetProcMeminfoTimeoutS']

    if ini.OPTIONS_FROM_FILE is not None:
        report(f"\nOverriding default options via {ini_file!r}:")
        for k, v in ini.OPTIONS_FROM_FILE.items():
            report(f"    {k}={v}")

    report(f"\nConfiguration defined in {conf_file!r}:")
    for k, v in conf.options.items():
        report(f"    {k}={v!r}")

    return conf, ini


def to_megabytes(bytes_count):
    return bytes_count // (1024 * 1024)


def to_percentage(share_0_1):
    return round(share_0_1 * 100)


def box_combined_cpu_usage(box):
    cpu_usage_file = '/proc/loadavg'
    cpu_usage_reply = box.eval(f'cat {cpu_usage_file}')
    cpu_usage_reply_list = cpu_usage_reply.split() if cpu_usage_reply else None

    if not cpu_usage_reply_list or len(cpu_usage_reply_list) < 1:
        # TODO: Properly log an arbitrary string in cpu_usage_reply.
        raise exceptions.BoxFileContentError(cpu_usage_file)

    try:
        # Get CPU usage average during the last minute.
        result = float(cpu_usage_reply_list[0])
    except ValueError:
        raise exceptions.BoxFileContentError(cpu_usage_file)

    return result


def _check_storages(api, ini, camera_count):
    space_for_recordings_bytes = (
        camera_count
        * ini['minimumArchiveFreeSpacePerCameraSeconds']
        * ini['archiveBitratePerCameraMbps'] * 1000 * 1000 // 8
    )
    for storage in _get_storages(api):
        if storage.free_space >= storage.reserved_space + space_for_recordings_bytes:
            break
    else:
        raise exceptions.BoxStateError(
            f"Server has no video archive Storage "
            f"with at least {space_for_recordings_bytes // 1024 ** 2} MB "
            f"of non-reserved free space.")


def get_cumulative_swap_kilobytes(box):
    output = box.eval('cat /proc/vmstat')
    raw = [line.split() for line in output.splitlines()]
    try:
        data = {k: int(v) for k, v in raw}
    except ValueError:
        return None
    try:
        pages_swapped = data['pswpout']
    except KeyError:
        return None
    return pages_swapped * 4  # All modern OSes operate with 4K pages.


class _BoxCpuTimes:
    def __init__(self, box: BoxConnection, cpu_cores):
        content = box.eval('cat /proc/uptime')
        if content is None:
            raise exceptions.BoxFileContentError('/proc/uptime')
        components = content.split()
        try:
            self.uptime_s = float(components[0])
            idle_time_s = float(components[1])
        except ValueError:
            raise exceptions.BoxFileContentError('/proc/uptime')
        self.busy_time_s = self.uptime_s - idle_time_s / cpu_cores

    def cpu_usage(self, prev: '_BoxCpuTimes'):
        value = (self.busy_time_s - prev.busy_time_s) / (self.uptime_s - prev.uptime_s)
        if value > 1:
            return 1
        return value


def box_tx_rx_errors(box):
    errors_content = box.eval(f'cat /sys/class/net/{box.eth_name}/statistics/tx_errors /sys/class/net/{box.eth_name}/statistics/rx_errors')

    if errors_content is None:
        raise exceptions.BoxFileContentError(f'/sys/class/net/{box.eth_name}/statistics/{{tx,rx}}_errors')

    errors_components = errors_content.split('\n')
    tx_errors, rx_errors = [int(v) for v in errors_components[0:2]]

    return tx_errors + rx_errors


def report(message):
    print(message, flush=True)
    if message.strip():
        logging.info('[report] ' + message.strip('\n'))


# Refers to the log file in the messages to the user. Filled after determining the log file path.
log_file_ref = None


def _wait_for_api(api, ini):
    started_at = time.time()

    while True:
        resp = api.ping()

        if resp and resp.code == 200 and resp.payload.get('error', None) == '0':
            break

        if time.time() - started_at > ini['apiReadinessTimeoutSeconds']:
            return False

        time.sleep(1)
    return True


def _test_api(api, ini):
    logging.info('Starting REST API basic test...')
    if not _wait_for_api(api, ini):
        raise exceptions.ServerApiError("API of Server is not working: ping request was not successful.")
    report('\nREST API basic test is OK.')
    logging.info('Starting REST API authentication test...')
    api.check_authentication()
    report('REST API authentication test is OK.')


class Storage:
    def __init__(self, raw):
        try:
            self.url = raw['url']
            self.free_space = int(raw['freeSpace'])
            self.reserved_space = int(raw['reservedSpace'])
            flags_joined = raw['storageStatus']
            if flags_joined == '' or flags_joined == 'none':
                flags = []
            else:
                flags = flags_joined.split('|')
            id = uuid.UUID(raw['storageId'])
            self.initialized = all((
                id != uuid.UUID(int=0),
                'beingChecked' not in flags,
                self.free_space >= 0,
            ))
        except Exception as e:
            raise exceptions.ServerApiError(
                'Incorrect Storage info received from the Server.',
                original_exception=e)


def _get_storages(api) -> List[Storage]:
    for attempt in range(1, 6):
        logging.info(f"Try to get Storages, attempt #{attempt}...")
        try:
            reply = api.get_storage_spaces()
        except Exception as e:
            raise exceptions.ServerApiError(
                'Unable to get Server Storages via REST HTTP: request failed.',
                original_exception=e)
        if reply is None:
            raise exceptions.ServerApiError(
                'Unable to get Server Storages via REST HTTP: invalid reply.')
        if reply:
            storages = [Storage(item) for item in reply]
            if all(storage.initialized for storage in storages):
                return storages
        time.sleep(3)
    raise exceptions.ServerApiError(
        'Unable to get Server Storages via REST HTTP: not all Storages are initialized.')


_rtsp_perf_frame_regex = re.compile(
    r'.+'
    r'vms_benchmark_stream_id=(?P<stream_id>[-\w]+)'
    r'.+'
    r'timestamp (?P<timestamp>\d+) us')

_rtsp_perf_summary_regex = re.compile(
    r'.+'
    r'Total bitrate (?P<bitrate>\d+\.\d+) MBit/s, '
    r'working sessions (?P<working>\d+), '
    r'failed (?P<failed>\d+), '
    r'bytes read (?P<bytes>\d+)'
)


def _rtsp_perf_frames(stdout, output_file_path):
    if output_file_path:
        output_file = open(output_file_path, "w")
        report(f'INI: Going to log rtsp_perf stdout lines to {output_file_path}')
    else:
        output_file = None

    while True:
        line = stdout.readline().decode('UTF-8').strip('\n')

        if output_file:
            output_file.write(line.strip() + '\n')

        warning_prefix = 'WARNING: '
        if line.startswith(warning_prefix):
            raise exceptions.RtspPerfError("Streaming error: " + line[len(warning_prefix):])

        match_res = _rtsp_perf_frame_regex.match(line)
        if match_res is not None:
            yield match_res.group('stream_id'), int(match_res.group('timestamp'))
            continue

        match_res = _rtsp_perf_summary_regex.match(line)
        if match_res is not None:
            if int(match_res.group('failed')) > 0:
                raise exceptions.RtspPerfError("Streaming error: Some RTSP sessions failed")
            continue

        logging.info(f"Unrecognized line from rtsp_perf: {line}")


class _StreamTypeStats:
    def __init__(self, type, ini):
        self._type = type
        self._ini = ini
        self.frame_drops = 0
        self.worst_lag_us = 0
        self._first_timestamp_us_by_stream_id = {}
        self._first_pts_us_by_stream_id = {}
        self._last_pts_us_by_stream_id = {}
        self._expected_pts_diff_us = 1_000_000 // ini['testStreamFpsHigh']

    def update_with_new_frame(self, camera_id, stream_id, pts_us, current_timestamp_s):
        self._update_max_lag(pts_us, current_timestamp_s, stream_id)
        self._update_frame_drops(camera_id, stream_id, pts_us)
        self._last_pts_us_by_stream_id[stream_id] = pts_us

    def _update_frame_drops(self, camera_id, pts_stream_id, pts_us):
        if pts_stream_id not in self._last_pts_us_by_stream_id:
            return
        pts_diff_us = pts_us - self._last_pts_us_by_stream_id[pts_stream_id]
        pts_diff_deviation_us = abs(pts_diff_us - self._expected_pts_diff_us)
        if pts_diff_deviation_us > self._ini['maxAllowedPtsDiffDeviationUs']:
            self.frame_drops += 1
            logging.info(
                f'Detected frame drop on {self._type} stream '
                f'from camera {camera_id}: '
                f'diff {pts_diff_us} us '
                f'deviates from the expected {self._expected_pts_diff_us} us '
                f'by {pts_diff_deviation_us} us; '
                f'PTS {pts_us} us.'
            )

    def _update_max_lag(self, pts_us, current_timestamp_s, pts_stream_id):
        current_timestamp_us = int(current_timestamp_s * 1_000_000)
        if pts_stream_id not in self._first_timestamp_us_by_stream_id:
            self._first_timestamp_us_by_stream_id[pts_stream_id] = current_timestamp_us
            self._first_pts_us_by_stream_id[pts_stream_id] = pts_us
            return
        relative_pts_us = pts_us - self._first_pts_us_by_stream_id[pts_stream_id]
        expected_relative_pts_us = (
            current_timestamp_us - self._first_timestamp_us_by_stream_id[pts_stream_id])
        lag_us = expected_relative_pts_us - relative_pts_us  # Positive lag: the stream is delayed.
        if abs(lag_us) > abs(self.worst_lag_us):
            self.worst_lag_us = lag_us


class _BoxPoller:
    def __init__(self, api, box, cpu_cores):
        self._api = api
        self._box = box
        self._cpu_cores = cpu_cores
        self._results = []
        self.stop_event = threading.Event()
        self.exception = None
        self._start_timestamp_ms = round(time.time() * 1000)
        self._thread = threading.Thread(target=self._target)
        self._thread.start()

    def _count_storage_failures(self):
        logging.info("Requesting potential failure events from the Server...")
        log_events = self._api.get_events(self._start_timestamp_ms)
        storage_failure_event_count = sum(
            event['aggregationCount']
            for event in log_events
            if event['eventParams'].get('eventType', None) == 'storageFailureEvent'
        )
        logging.info(f"Storage failures: {storage_failure_event_count}")
        return storage_failure_event_count

    def _target(self):
        try:
            while not self.stop_event.isSet():
                self.stop_event.wait(60)
                cpu_times = _BoxCpuTimes(self._box, self._cpu_cores)
                storage_failure_event_count = self._count_storage_failures()
                tx_rx_errors = box_tx_rx_errors(self._box)
                swapped_kilobytes = get_cumulative_swap_kilobytes(self._box)
                self._results.append((
                    cpu_times,
                    storage_failure_event_count,
                    tx_rx_errors,
                    swapped_kilobytes,
                ))
        except Exception as e:
            self.exception = e

    def please_stop(self):
        self.stop_event.set()

    def is_alive(self):
        return self._thread.is_alive()

    def get_issues(self):
        if self.exception is None:
            return []
        if isinstance(self.exception, exceptions.VmsBenchmarkIssue):
            return [self.exception]
        return [exceptions.TestCameraStreamingIssue(
            'Unexpected error during acquiring VMS Server CPU usage. '
            'Can be caused by network issues or Server issues.',
            original_exception=self.exception)]

    def get_results(self):
        return self._results.pop()


def _run_load_tests(api, box, box_platform, conf, ini, vms):
    report('')
    report('Starting load tests...')
    load_test_started_at_s = time.time()

    swapped_initially_kilobytes = get_cumulative_swap_kilobytes(box)
    if swapped_initially_kilobytes is None:
        report("Cannot obtain swap information.")

    for [test_number, test_camera_count] in zip(itertools.count(1, 1), conf['virtualCameraCount']):
        ram_free_bytes = _obtain_and_check_box_ram_free_bytes(box_platform, ini, test_camera_count)

        total_live_stream_count = math.ceil(
            conf['liveStreamsPerCameraRatio'] * test_camera_count)
        total_archive_stream_count = math.ceil(
            conf['archiveStreamsPerCameraRatio'] * test_camera_count)
        report(
            f"\nLoad test #{test_number}: "
            f"{test_camera_count} virtual camera(s), "
            f"{total_live_stream_count} live stream(s), "
            f"{total_archive_stream_count} archive stream(s)...")

        logging.info(f'Starting {test_camera_count} virtual camera(s)...')
        ini_local_ip = ini['testcameraLocalInterface']
        test_camera_context_manager = test_camera_runner.test_camera_running(
            local_ip=ini_local_ip if ini_local_ip else box.local_ip,
            count=test_camera_count,
            primary_fps=ini['testStreamFpsHigh'],
            secondary_fps=ini['testStreamFpsLow'],
        )
        with test_camera_context_manager as test_camera_context:
            report(f"    Started {test_camera_count} virtual camera(s).")

            def wait_test_cameras_discovered(
                timeout, online_duration) -> Tuple[bool, Optional[List[Camera]]]:

                started_at = time.time()
                detection_started_at = None
                while time.time() - started_at < timeout:
                    if test_camera_context.poll() is not None:
                        raise exceptions.TestCameraError(
                            f'Virtual camera (testcamera) process exited unexpectedly with code '
                            f'{test_camera_context.returncode}')

                    cameras = api.get_test_cameras()

                    if len(cameras) >= test_camera_count:
                        if detection_started_at is None:
                            detection_started_at = time.time()
                        elif time.time() - detection_started_at >= online_duration:
                            return True, cameras
                    else:
                        detection_started_at = None

                    time.sleep(1)
                return False, None

            try:
                discovering_timeout_seconds = conf['cameraDiscoveryTimeoutSeconds']

                report(
                    "    Waiting for virtual camera discovery and going live "
                    f"(timeout is {discovering_timeout_seconds} s)..."
                )
                res, cameras = wait_test_cameras_discovered(
                    timeout=discovering_timeout_seconds, online_duration=3)
                if not res:
                    raise exceptions.TestCameraError('Timeout expired.')

                report("    All virtual cameras discovered and went live.")

                for camera in cameras:
                    if camera.enable_recording(highStreamFps=ini['testStreamFpsHigh']):
                        report(f"    Recording on camera {camera.id} enabled.")
                    else:
                        raise exceptions.TestCameraError(
                            f"Failed enabling recording on camera {camera.id}.")
            except Exception as e:
                raise exceptions.TestCameraError(
                    f"Not all virtual cameras were discovered or went live.", e) from e

            report(
                f"    Waiting for the archives to be ready for streaming "
                f"({ini['sleepBeforeCheckingArchiveSeconds']} s)...")
            time.sleep(ini['sleepBeforeCheckingArchiveSeconds'])

            archive_start_time_ms = api.get_archive_start_time_ms(cameras[0].id)
            archive_read_pos_ms_utc = archive_start_time_ms + ini['archiveReadingPosS'] * 1000

            report('    Streaming test...')

            stream_reader_context_manager = stream_reader_runner.stream_reader_running(
                camera_ids=[camera.id for camera in cameras],
                total_live_stream_count=total_live_stream_count,
                total_archive_stream_count=total_archive_stream_count,
                user=conf['vmsUser'],
                password=conf['vmsPassword'],
                box_ip=box.ip,
                vms_port=vms.port,
                archive_read_pos_ms_utc=archive_read_pos_ms_utc,
            )
            with stream_reader_context_manager as stream_reader_context:
                stream_reader_process = stream_reader_context[0]
                rtsp_perf_frames = _rtsp_perf_frames(
                    stream_reader_process.stdout, ini["rtspPerfLinesOutputFile"])
                streams = stream_reader_context[1]

                started = False
                stream_opening_started_at_s = time.time()

                streams_started_flags = dict((stream_id, False) for stream_id, _ in streams.items())

                while time.time() - stream_opening_started_at_s < 25:
                    if stream_reader_process.poll() is not None:
                        raise exceptions.RtspPerfError(
                            "Can't open streams or streaming unexpectedly ended.")

                    stream_id, _ = next(rtsp_perf_frames)

                    if stream_id not in streams_started_flags:
                        raise exceptions.RtspPerfError(
                            f'Cannot open video streams: unexpected stream id.')

                    streams_started_flags[stream_id] = True

                    if len(list(filter(lambda e: e[1] is False, streams_started_flags.items()))) == 0:
                        started = True
                        break

                if started is False:
                    # TODO: check all streams are opened
                    raise exceptions.TestCameraStreamingIssue(
                        f'Cannot open video streams: timeout expired.')

                report("    All streams opened.")

                streaming_duration_mins = conf['streamingTestDurationMinutes']
                streaming_test_started_at_s = time.time()

                stream_stats = {
                    'live': _StreamTypeStats('live', ini),
                    'archive': _StreamTypeStats('archive', ini),
                }
                streaming_ended_expectedly = False
                issues = []
                first_cpu_times = last_cpu_times = _BoxCpuTimes(box, box_platform.cpu_count)
                first_tx_rx_errors = last_tx_rx_errors = box_tx_rx_errors(box)
                cpu_usage_max = 0
                box_poller = _BoxPoller(api, box, box_platform.cpu_count)

                try:
                    while True:
                        if stream_reader_process.poll() is not None:
                            raise exceptions.RtspPerfError("Streaming unexpectedly ended.")

                        if not box_poller.is_alive():
                            issues.extend(box_poller.get_issues())
                            streaming_ended_expectedly = True
                            break

                        pts_stream_id, pts_us = next(rtsp_perf_frames)
                        timestamp_s = time.time()
                        stream_type = streams[pts_stream_id]['type']
                        camera_id = streams[pts_stream_id]["camera_id"]
                        stream_stats[stream_type].update_with_new_frame(
                            camera_id, pts_stream_id, pts_us, timestamp_s)

                        try:
                             [cpu_times, storage_failure_event_count, tx_rx_errors, swapped_kilobytes] = box_poller.get_results()
                        except LookupError:
                            continue
                        cpu_usage_last_minute = cpu_times.cpu_usage(last_cpu_times)
                        cpu_usage_max = max(cpu_usage_max, cpu_usage_last_minute)
                        last_cpu_times = cpu_times
                        last_tx_rx_errors = tx_rx_errors

                        live_worst_lag_s = stream_stats['live'].worst_lag_us / 1_000_000
                        archive_worst_lag_s = stream_stats['archive'].worst_lag_us / 1_000_000
                        report(
                            f"    {round(time.time() - streaming_test_started_at_s)} seconds passed; "
                            f"box CPU usage: {round(cpu_usage_last_minute * 100)}%, "
                            f"dropped frames: "
                            f"{stream_stats['live'].frame_drops} (live), "
                            f"{stream_stats['archive'].frame_drops} (archive), "
                            f"worst stream lag: "
                            f"{live_worst_lag_s:.3f} s (live), "
                            f"{archive_worst_lag_s:.3f} s (archive).")

                        if cpu_usage_last_minute > ini['cpuUsageThreshold']:
                            issues.append(exceptions.CpuUsageThresholdExceededIssue(
                                cpu_usage_last_minute,
                                ini['cpuUsageThreshold']
                            ))

                        if storage_failure_event_count > 0:
                            issues.append(exceptions.StorageFailuresIssue(storage_failure_event_count))

                        if swapped_initially_kilobytes is not None:
                            swapped_during_test_kilobytes = swapped_kilobytes - swapped_initially_kilobytes
                            if swapped_during_test_kilobytes > ini['swapThresholdKilobytes']:
                                issues.append(exceptions.BoxStateError(
                                    f"More than {ini['swapThresholdKilobytes']} KB swapped at the "
                                    f"box during the tests: {swapped_during_test_kilobytes} KB."))

                        for stream_type in 'live', 'archive':
                            if stream_stats[stream_type].frame_drops > ini['maxAllowedFrameDrops']:
                                issues.append(exceptions.VmsBenchmarkIssue(
                                    f'{stream_stats[stream_type].frame_drops} frame drops detected '
                                    f'in {stream_type} streams.'))
                            if stream_stats[stream_type].worst_lag_us > ini['worstAllowedStreamLagUs']:
                                worst_lag_s = stream_stats[stream_type].worst_lag_us / 1_000_000
                                issues.append(exceptions.TestCameraStreamingIssue(
                                    f'Lag of one of {stream_type} streams '
                                    f'reached {worst_lag_s :.3f} seconds.'
                                ))

                        if issues:
                            streaming_ended_expectedly = True
                            report(
                                f"    Streaming test #{test_number} with {test_camera_count} virtual camera(s) aborted because of issues.")
                            break

                        if timestamp_s - streaming_test_started_at_s > streaming_duration_mins * 60:
                            streaming_ended_expectedly = True
                            report(
                                f"    Streaming test #{test_number} with {test_camera_count} virtual camera(s) finished.")
                            break
                finally:
                    box_poller.please_stop()

                if last_cpu_times is not first_cpu_times:
                    cpu_usage_avg = last_cpu_times.cpu_usage(first_cpu_times)
                else:
                    cpu_usage_avg = None

                if not streaming_ended_expectedly:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: '
                        'the stream has unexpectedly finished; '
                        'can be caused by network issues or Server issues.',
                        original_exception=issues
                    )

                if time.time() - timestamp_s > 5:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: '
                        'the stream has hung; '
                        'can be caused by network issues or Server issues.')

                try:
                    ram_free_bytes = box_platform.obtain_ram_free_bytes()
                except exceptions.VmsBenchmarkError as e:
                    issues.append(exceptions.UnableToFetchDataFromBox(
                        'Unable to fetch box RAM usage',
                        original_exception=e
                    ))

                if last_tx_rx_errors is not None and first_tx_rx_errors is not None:
                    tx_rx_errors_during_test = last_tx_rx_errors - first_tx_rx_errors
                else:
                    tx_rx_errors_during_test = None

                streaming_test_duration_s = round(time.time() - streaming_test_started_at_s)
                report(
                    f"    Streaming test statistics:\n"
                    f"        Frame drops in live stream: {stream_stats['live'].frame_drops} (expected 0)\n"
                    f"        Frame drops in archive stream: {stream_stats['archive'].frame_drops} (expected 0)\n"
                    + (
                        f"        Box network errors: {tx_rx_errors_during_test} (expected 0)\n"
                        if tx_rx_errors_during_test is not None else '')
                    + (
                        f"        Maximum box CPU usage: {cpu_usage_max * 100:.0f}%\n"
                        if cpu_usage_max is not None else '')
                    + (
                        f"        Average box CPU usage: {cpu_usage_avg * 100:.0f}%\n"
                        if cpu_usage_avg is not None else '')
                    + (
                        f"        Box free RAM: {to_megabytes(ram_free_bytes)} MB\n"
                        if ram_free_bytes is not None else '')
                    + f"        Streaming test duration: {streaming_test_duration_s} s"
                )

                if len(issues) > 0:
                    raise exceptions.VmsBenchmarkIssue(f'{len(issues)} issue(s) detected:', sub_issues=issues)

                report(f"    Streaming test #{test_number} with {test_camera_count} virtual camera(s) succeeded.")

    report(
        f'\nLoad tests finished successfully; '
        f'duration: {int(time.time() - load_test_started_at_s)} s.')


def _obtain_and_check_box_ram_free_bytes(box_platform, ini, test_camera_count):
    ram_free_bytes = box_platform.obtain_ram_free_bytes()
    ram_required_bytes = test_camera_count * ini['ramPerCameraMegabytes'] * 1024 * 1024

    if ram_free_bytes < ram_required_bytes:
        raise exceptions.InsufficientResourcesError(
            f"Not enough free RAM on the box for {test_camera_count} cameras.")

    return ram_free_bytes


def _test_vms(api, box, box_platform, conf, ini, vms):
    ram_free_bytes = box_platform.obtain_ram_free_bytes()
    report(f"Box RAM free: {to_megabytes(ram_free_bytes)} MB "
        f"of {to_megabytes(box_platform.ram_bytes)} MB")
    _check_storages(api, ini, camera_count=max(conf['virtualCameraCount']))
    for i in 1, 2, 3:
        cameras = api.get_test_cameras_all()
        if cameras is not None:
            break
        report(f"Attempt #{i} to get camera list")
        time.sleep(i)
    if cameras is not None:
        for camera in cameras:
            if not api.remove_camera(camera.id):
                raise exceptions.ServerApiError(
                    message=f"Unable to remove camera with id={camera.id}"
                )
    else:
        raise exceptions.ServerApiError(message="Unable to get camera list.")
    _run_load_tests(api, box, box_platform, conf, ini, vms)


def _obtain_box_platform(box, linux_distribution):
    box_platform = BoxPlatform.create(box, linux_distribution)

    report(
        "\nBox properties detected:\n"
        f"    IP address: {box.ip}\n"
        f"    Network adapter name: {box.eth_name}\n"
        f"    Network adapter bandwidth: {(box.eth_speed + ' Mbps') if box.eth_speed else 'Unknown'}\n"
        f"    SSH user is{'' if box.is_root else ' not'} root.\n"
        f"    Linux distribution name: {linux_distribution.name}\n"
        f"    Linux distribution version: {linux_distribution.version}\n"
        f"    Linux kernel version: {'.'.join(str(c) for c in linux_distribution.kernel_version)}\n"
        f"    Arch: {box_platform.arch}\n"
        f"    Number of CPUs: {box_platform.cpu_count}\n"
        f"    CPU features: {', '.join(box_platform.cpu_features) if len(box_platform.cpu_features) > 0 else 'None'}\n"
        f"    RAM: {to_megabytes(box_platform.ram_bytes)} MB "
        f"({to_megabytes(box_platform.obtain_ram_free_bytes())} MB free)\n"
        "    File systems:\n"
        + '\n'.join(
            f"        {storage['fs']} "
            f"on {storage['point']}: "
            f"free {int(storage['space_free']) / 1024 / 1024 / 1024:.1f} GB "
            f"of {int(storage['space_total']) / 1024 / 1024 / 1024:.1f} GB"
            for (point, storage) in box_platform.storages_list.items())
    )

    return box_platform


def _check_time_diff(box, ini):
    box_time_output = box.eval('date +%s.%N')
    if not box_time_output:
        raise exceptions.BoxCommandError('Unable to get current box time using the `date` command.')
    try:
        box_time = float(box_time_output.strip())
    except ValueError:
        raise exceptions.BoxStateError("Cannot parse output of the `date` command.")
    host_time = time.time()
    logging.info(f"Time difference (box time minus host time): {box_time - host_time:.3f} s.")
    if abs(box_time - host_time) > ini['timeDiffThresholdSeconds']:
        raise exceptions.BoxStateError(
            f"The box time differs from the host time by more than {ini['timeDiffThresholdSeconds']} s.")


def _obtain_running_vms(box, linux_distribution):
    vmses = VmsScanner.scan(box, linux_distribution)
    if not vmses:
        raise exceptions.BoxStateError("No Server installations found at the box.")
    report(
        f"\nDetected Server installation(s):\n"
        + '\n'.join(
            f"    {vms.customization} in {vms.dir} ("
            f"port {vms.port or 'N/A'}, "
            f"pid {vms.pid or 'N/A'}, "
            f"uid {vms.uid or 'N/A'})"
            for vms in vmses)
    )
    if len(vmses) > 1:
        raise exceptions.BoxStateError("More than one Server installation found at the box.")
    vms = vmses[0]
    if not vms.is_up():
        raise exceptions.BoxStateError("Server is not running currently at the box.")
    return vms


def _obtain_restarted_vms(box, linux_distribution):
    timeout = 30
    started_at = time.time()

    while True:
        vmses = VmsScanner.scan(box, linux_distribution)

        if vmses and len(vmses) > 0 and vmses[0].is_up():
            break

        if time.time() - started_at > timeout:
            raise exceptions.ServerError("Unable to restart Server: Server was not upped.")

        time.sleep(0.5)

    vms = vmses[0]
    return vms


def _override_ini_config(vms, ini):
    high_stream_interval_us = 1000000 // ini['testStreamFpsHigh']
    modulus_us = ini['testFileHighDurationMs'] * 1000 + high_stream_interval_us
    vms.override_ini_config({
        'nx_streaming': {
            'enableTimeCorrection': 0,
            'unloopCameraPtsWithModulus': modulus_us,
        },
    })


def _connect_to_box(conf, conf_file):
    password = conf.get('boxPassword', None)
    if password and platform.system() == 'Linux':
        res = host_tests.SshPassInstalled().call()

        if not res.success:
            raise exceptions.HostPrerequisiteFailed(
                "sshpass is not on PATH"
                " (check if it is installed; to install on Ubuntu: `sudo apt install sshpass`)."
                f"Details for the error: {res.formatted_message()}"
            )
    box = BoxConnection(
        host=conf['boxHostnameOrIp'],
        login=conf.get('boxLogin', None),
        password=password,
        port=conf['boxSshPort'],
        conf_file=conf_file
    )
    host_key = box_tests.SshHostKeyIsKnown(box, conf_file).call()
    if host_key is not None:
        box.supply_host_key(host_key)
    box.obtain_connection_info()
    if not box.is_root:
        res = box_tests.SudoConfigured(box).call()

        if not res.success:
            raise exceptions.SshHostKeyObtainingFailed(
                'Sudo is not configured properly, check that user is root or can run `sudo true` '
                'without typing a password.\n'
                f"Details of the error: {res.formatted_message()}"
            )
    return box


@click.command(context_settings=dict(help_option_names=["-h", "--help"]))
@click.option(
    '--config', '-c', 'conf_file', default='vms_benchmark.conf', metavar='<filename>', show_default=True,
    help='Configuration file.')
@click.option(
    '--ini-config', '-i', 'ini_file', default='vms_benchmark.ini', metavar='<filename>', show_default=True,
    help='Internal configuration file for experimenting and debugging.')
@click.option(
    '--log', '-l', 'log_file', default='vms_benchmark.log', metavar='<filename>', show_default=True,
    help='Detailed log of all actions; intended to be studied by the support team.')
def main(conf_file, ini_file, log_file):
    global log_file_ref
    log_file_ref = repr(log_file)
    logging.basicConfig(filename=log_file, filemode='w', level=logging.DEBUG,
        format='%(asctime)s %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    report(f"VMS Benchmark started; logging to {log_file_ref}.")

    conf, ini = load_configs(conf_file, ini_file)

    box = _connect_to_box(conf, conf_file)
    linux_distribution = LinuxDistributionDetector.detect(box)
    box_platform = _obtain_box_platform(box, linux_distribution)
    _check_time_diff(box, ini)

    vms = _obtain_running_vms(box, linux_distribution)
    vms.dismount_ini_dirs()
    try:
        api = ServerApi(box.ip, vms.port, user=conf['vmsUser'], password=conf['vmsPassword'])
        _test_api(api, ini)

        storages = _get_storages(api)
        _stop_vms(vms)
        _override_ini_config(vms, ini)
        _clear_storages(box, storages, conf)
        vms = _restart_vms(api, box, linux_distribution, vms, ini)

        _test_vms(api, box, box_platform, conf, ini, vms)

        report('\nSUCCESS: All tests finished.')
    finally:
        vms.dismount_ini_dirs()


def _restart_vms(api, box, linux_distribution, vms, ini):
    report('Starting Server...')
    vms.start(exc=True)
    vms = _obtain_restarted_vms(box, linux_distribution)
    _wait_for_api(api, ini)
    report('Server started.')
    return vms


def _stop_vms(vms):
    report('Stopping server...')
    vms.stop(exc=True)
    report('Server stopped.')


def _clear_storages(box, storages: List[Storage], conf):
    report('Deleting Server video archives...')
    for storage in storages:
        box.sh(
            f"rm -rf "
            f"'{storage.url}/hi_quality' "
            f"'{storage.url}/low_quality' "
            f"'{storage.url}/'*_media.nxdb",
            timeout_s=conf['archiveDeletingTimeoutSeconds'], su=True, exc=True)
    report('Server video archives deleted.')


def _get_cameras_reliably(api):
    for i in 1, 2, 3:
        cameras = api.get_test_cameras_all()
        if cameras is not None:
            break
        report(f"Attempt #{i}to get camera list")
        time.sleep(i)
    if cameras is None:
        raise exceptions.ServerApiError(message="Unable to get camera list.")
    return cameras


def _format_exception(exception):
    if isinstance(exception, ValueError):
        return f"Invalid value: {exception}"
    elif isinstance(exception, KeyError):
        return f"Missing key: {exception}"
    elif isinstance(exception, urllib.error.HTTPError):
        if exception.code == 401:
            return 'Server refuses passed credentials: check .conf options vmsUser and vmsPassword'
        else:
            return f'Unexpected HTTP request error (code {exception.code})'
    else:
        return str(exception)


def _do_report_exception(exception, recursive_level, prefix=''):
    indent = '    ' * recursive_level
    if isinstance(exception, VmsBenchmarkError):
        report(f"{indent}{prefix}{str(exception)}")
        if isinstance(exception, VmsBenchmarkIssue):
            for sub_issue in exception.sub_issues:
                _do_report_exception(sub_issue, recursive_level=recursive_level + 1)
        if exception.original_exception:
            report(f'{indent}Caused by:')
            if isinstance(exception.original_exception, list):
                sub_exceptions = exception.original_exception
            else:
                sub_exceptions = [exception.original_exception]
            for sub_exception in sub_exceptions:
                _do_report_exception(sub_exception, recursive_level=recursive_level + 1)
    else:
        report(f'{indent}{prefix or "ERROR: "}{_format_exception(exception)}')


def report_exception(context_name, exception, note=None):
    prefix = '\n' + context_name + ': '
    _do_report_exception(exception, recursive_level=0, prefix=prefix)
    if note:
        report(f'\nNOTE: {note}')
    logging.exception(f'Exception yielding the above {context_name}:')


if __name__ == '__main__':
    try:
        try:
            main()
            logging.info(f'VMS Benchmark finished successfully.')
            sys.exit(0)
        except (VmsBenchmarkIssue, urllib.error.HTTPError) as e:
            report_exception('ISSUE', e,
                'Can be caused by network issues, or poor performance of the box or the host.')
        except VmsBenchmarkError as e:
            report_exception('ERROR', e,
                f'Technical details may be available in {log_file_ref}.' if log_file_ref else None)
        except Exception as e:
            report_exception(f'UNEXPECTED ERROR', e,
                f'Details may be available in {log_file_ref}.' if log_file_ref else None)

        logging.info(f'VMS Benchmark finished with an exception.')
    except Exception as e:
        sys.stderr.write(
            f'INTERNAL ERROR: {e}\n'
            f'\n'
            f'Please send the console output ' +
            (f'and {log_file_ref} ' if log_file_ref else '') + 'to the support team.\n')

    sys.exit(1)
