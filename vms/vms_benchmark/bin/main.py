#!/usr/bin/env python3
import itertools
import logging
import math
import platform
import re
import signal
import sys
import os
import time
import uuid
from typing import List, Tuple, Optional
from pathlib import Path

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
from vms_benchmark import service_objects
from vms_benchmark import box_tests
from vms_benchmark import host_tests
from vms_benchmark import exceptions

vms_version = None

try:
    from vms_benchmark_release.version import vms_version
except ImportError:
    def assign_vms_version():
        def _vms_version():
            return 'development'
        global vms_version
        vms_version = _vms_version
    assign_vms_version()

import urllib
import urllib.error
import click
import threading


conf_definition = {
    "boxHostnameOrIp": {"type": "str"},
    "boxLogin": {"type": "str", "default": ""},
    "boxPassword": {"type": "str", "default": ""},
    "boxSshPort": {"type": "int", "range": [1, 65535], "default": 22},
    "boxTelnetPort": {"type": "int", "range": [1, 65535], "default": 23},
    "boxSshKey": {"type": "str", "default": ""},
    "boxConnectByTelnet": {"type": "bool", "default": False},
    "vmsUser": {"type": "str"},
    "vmsPassword": {"type": "str"},
    "archiveDeletingTimeoutSeconds": {"type": "int", "range": [0, None], "default": 60},
}

ini_definition = {
    "virtualCameraCount": {"type": "intList", "range": [1, 999], "default": [4]},
    "liveStreamsPerCameraRatio": {"type": "float", "range": [0.0, 999.0], "default": 1.0},
    "archiveStreamsPerCameraRatio": {"type": "float", "range": [0.0, 999.0], "default": 0.2},
    "streamingTestDurationMinutes": {"type": "int", "range": [1, None], "default": 4 * 60},
    "cameraDiscoveryTimeoutSeconds": {"type": "int", "range": [0, None], "default": 3 * 60},
    "testcameraBin": {"type": "str", "default": "./tools/bin/testcamera"},
    "rtspPerfBin": {"type": "str", "default": "./tools/bin/rtsp_perf"},
    "plinkBin": {"type": "str", "default": "./tools/bin/plink"},
    "testFileHighResolution": {"type": "str", "default": "./high.ts"},
    "testFileHighPeriodUs": {"type": "int", "range": [1, None], "default": 10033334},
    "testFileLowResolution": {"type": "str", "default": "./low.ts"},
    "testFileLowPeriodUs": {"type": "int", "range": [1, None], "default": 10000000},
    "testStreamFpsHigh": {"type": "int", "range": [1, 999], "default": 30},
    "testStreamFpsLow": {"type": "int", "range": [1, 999], "default": 7},
    "enableSecondaryStream": {"type": "bool", "default": True},
    "testcameraOutputFile": {"type": "str", "default": ""},
    "testcameraFrameLogFile": {"type": "str", "default": ""},
    "testcameraExtraArgs": {"type": "str", "default": ""},
    "testcameraLocalInterface": {"type": "str", "default": ""},
    "cpuUsageThreshold": {"type": "float", "range": [0.0, 1.0], "default": 0.5},
    "archiveBitratePerCameraMbps": {"type": "int", "range": [1, 999], "default": 10},
    "minimumArchiveFreeSpacePerCameraSeconds": {"type": "int", "range": [1, None], "default": 240},
    "timeDiffThresholdSeconds": {"type": "float", "range": [0.0, None], "default": 3},
    "swapThresholdKilobytes": {"type": "int", "range": [0, None], "default": 0},
    "enableSwapThreshold": {"type": "bool", "default": False},
    "sleepBeforeCheckingArchiveSeconds": {"type": "int", "range": [0, None], "default": 100},
    "maxAllowedFrameDrops": {"type": "int", "range": [0, None], "default": 0},
    "ramPerCameraMegabytes": {"type": "int", "range": [1, None], "default": 40},
    "boxCommandTimeoutS": {"type": "int", "range": [1, None], "default": 15},
    "boxServiceCommandTimeoutS": {"type": "int", "range": [1, None], "default": 30},
    "boxGetFileContentTimeoutS": {"type": "int", "range": [1, None], "default": 30},
    "boxGetProcMeminfoTimeoutS": {"type": "int", "range": [1, None], "default": 10},
    "rtspPerfLinesOutputFile": {"type": "str", "default": ""},
    "rtspPerfStderrFile": {"type": "str", "default": ""},
    "archiveReadingPosS": {"type": "int", "range": [0, None], "default": 15},
    "apiReadinessTimeoutSeconds": {"type": "int", "range": [1, None], "default": 30},
    "worstAllowedStreamLagUs": {"type": "int", "range": [0, None], "default": 5_000_000},
    "maxAllowedPtsDiffDeviationUs": {"type": "int", "range": [0, None], "default": 2000},
    "unloopViaTestcamera": {"type": "bool", "default": True},
    "reportingPeriodSeconds": {"type": "int", "default": 60},
    "addCamerasManually": {"type": "bool", "default": False},
    "getStoragesMaxAttempts": {"type": "int", "default": 10},
    "getStoragesAttemptIntervalSeconds": {"type": "int", "default": 3},
    "boxTelnetConnectionWarningThresholdMs": {"type": "int", "default": 1000},
    "plinkConnectionTimeoutS": {"type": "int", "default": 21},
}


def _load_config_file(config_path, default_path, config_definition, config_required):
    benchmark_bin_path = Path(os.path.realpath(sys.argv[0])).parent
    config_file_path = Path(config_path or default_path)

    if config_file_path.is_absolute():
        if config_file_path.exists():
            config_file_path_final = config_file_path
        else:
            raise exceptions.VmsBenchmarkError(
                f'Unable to find the config file {config_file_path!r}.'
            )
    else:
        if config_file_path.exists():
            config_file_path_final = config_file_path
        elif (benchmark_bin_path / config_file_path).exists():
            config_file_path_final = benchmark_bin_path / config_file_path
        else:
            # Check that the config file is required or was specified by the user explicitly.
            if config_required or config_path:
                raise exceptions.VmsBenchmarkError(
                    f'Unable to find the config file {config_file_path!r}'
                    ' either in the current directory or in the VMS Benchmark executable directory.'
                )
            else:
                # ConfigParser() requires config path to be passed even if the file is optional,
                # thus just pass the default path (it doesn't matter that the file with this path
                # doesn't exist).
                # TODO: Refactor ConfigParser() to remove this quirk.
                config_file_path_final = default_path

    return ConfigParser(
        config_file_path_final,
        config_definition,
        is_file_optional=not config_required
    )


def load_configs(conf_file, ini_file):
    conf_file_default = 'vms_benchmark.conf'
    ini_file_default = 'vms_benchmark.ini'
    benchmark_bin_path = Path(os.path.realpath(sys.argv[0])).parent

    conf = _load_config_file(conf_file, conf_file_default, conf_definition, True)
    ini = _load_config_file(ini_file, ini_file_default, ini_definition, False)

    def path_from_config(path):
        return path if Path(path).is_absolute() else str(benchmark_bin_path / path)

    test_camera_runner.ini_testcamera_bin = path_from_config(ini['testcameraBin'])
    test_camera_runner.ini_test_file_high_resolution = path_from_config(
        ini['testFileHighResolution'])
    test_camera_runner.ini_test_file_low_resolution = path_from_config(
        ini['testFileLowResolution'])
    test_camera_runner.ini_testcamera_output_file = ini['testcameraOutputFile']
    test_camera_runner.ini_testcamera_frame_log_file = ini['testcameraFrameLogFile']
    test_camera_runner.ini_testcamera_extra_args = ini['testcameraExtraArgs']
    test_camera_runner.ini_unloop_via_testcamera = ini['unloopViaTestcamera']
    test_camera_runner.ini_test_file_high_period_us = ini['testFileHighPeriodUs']
    test_camera_runner.ini_test_file_low_period_us = ini['testFileLowPeriodUs']
    test_camera_runner.ini_enable_secondary_stream = ini['enableSecondaryStream']
    stream_reader_runner.ini_rtsp_perf_bin = path_from_config(ini['rtspPerfBin'])
    stream_reader_runner.ini_rtsp_perf_stderr_file = ini['rtspPerfStderrFile']
    box_connection.ini_plink_bin = path_from_config(ini['plinkBin'])
    box_connection.ini_box_command_timeout_s = ini['boxCommandTimeoutS']
    box_connection.ini_box_get_file_content_timeout_s = ini['boxGetFileContentTimeoutS']
    vms_scanner.ini_box_service_command_timeout_s = ini['boxServiceCommandTimeoutS']
    box_platform.ini_box_get_proc_meminfo_timeout_s = ini['boxGetProcMeminfoTimeoutS']
    service_objects.ssh_host_key_obtainer.ini_plink_connection_timeout_s = (
        ini['plinkConnectionTimeoutS'])

    if ini.OPTIONS_FROM_FILE is not None:
        report(f"\nOverriding default options via {ini.filepath!r}:")
        isReported = False
        for k, v in ini.OPTIONS_FROM_FILE.items():
            isReported = True
            report(f"    {k}={v}")
        if not isReported:
            report(f"    No overriding values found in .ini file.")

    report(f"\nConfiguration defined in {conf.filepath!r}:")
    for k, v in conf.options.items():
        report(f"    {k}={v!r}")

    return conf, ini


def to_megabytes(bytes_count):
    return bytes_count // (1024 ** 2)


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
        * ini['archiveBitratePerCameraMbps'] * 1000 ** 2 // 8
    )
    for storage in _get_storages(api, ini):
        if (
            storage.is_used_for_writing and
                storage.free_space >= storage.reserved_space + space_for_recordings_bytes
        ):
            break
    else:
        raise exceptions.BoxStateError(
            f"Server has no video archive Storage "
            f"with at least {to_megabytes(space_for_recordings_bytes)} MB "
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
        if self.uptime_s - prev.uptime_s == 0:
            return None

        value = (self.busy_time_s - prev.busy_time_s) / (self.uptime_s - prev.uptime_s)
        if value > 1:
            return 1

        return value


def box_tx_rx_errors(box):
    errors_content = box.eval(
        f'cat /sys/class/net/{box.eth_name}/statistics/tx_errors /sys/class/net/{box.eth_name}/statistics/rx_errors')

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
        raise exceptions.ServerApiError(
            "API of Server is not working: ping request was not successful.")
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
            self.is_used_for_writing = raw['isUsedForWriting']
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


def _get_storages(api, ini) -> List[Storage]:
    for attempt in range(1, 1 + ini["getStoragesMaxAttempts"]):
        attempt_str = "" if attempt == 0 else f", attempt #{attempt}"
        logging.info(f"Try to get Storages{attempt_str}...")
        try:
            reply = api.get_storage_spaces()
        except Exception as e:
            raise exceptions.ServerApiError(
                "Unable to get Server Storages via REST HTTP: request failed.",
                original_exception=e)
        if reply is None:
            raise exceptions.ServerApiError(
                "Unable to get Server Storages via REST HTTP: invalid reply.")
        storages = [Storage(item) for item in reply if Storage(item).initialized]
        if storages:
            return storages
        time.sleep(ini["getStoragesAttemptIntervalSeconds"])

    raise exceptions.ServerApiError(
        "Unable to get Server Storages via REST HTTP: no Storages are initialized.")


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

_rtsp_perf_warning_regex = re.compile(
    r'.*WARNING: (?P<message>.*)'
)


def _rtsp_perf_frames(stdout, output_file_path):
    if output_file_path:
        output_file = open(output_file_path, "w")
        report(f'INI: Going to log rtsp_perf stdout lines to {output_file_path!r}')
    else:
        output_file = None

    while True:
        line = stdout.readline().decode('UTF-8').strip('\n\r')

        if output_file:
            timestamp_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
            if line.startswith(timestamp_str):  # Some rtsp_perf lines already have timestamps.
                prefix = ''
            else:
                prefix = timestamp_str + ' '
            output_file.write(f'{prefix}{line}\n')

        match_res = _rtsp_perf_warning_regex.match(line)
        if match_res is not None:
            raise exceptions.RtspPerfError(f'Streaming error: {match_res.group("message")}')

        match_res = _rtsp_perf_frame_regex.match(line)
        if match_res is not None:
            yield match_res.group('stream_id'), int(match_res.group('timestamp'))
            continue

        match_res = _rtsp_perf_summary_regex.match(line)
        if match_res is not None:
            if int(match_res.group('failed')) > 0:
                raise exceptions.RtspPerfError("Streaming error: Some RTSP sessions failed.")
            continue

        logging.info(f"Unrecognized line from rtsp_perf: {line!r}")


class _StreamTypeStats:
    def __init__(self, type, ini):
        self._type = type
        self._ini = ini
        self.frame_drops = 0
        self._min_lag_us = 0
        self._max_lag_us = 0
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
                f'Detected frame-dropping situation(s) on {self._type} stream '
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
        self._min_lag_us = min(self._min_lag_us, lag_us)
        self._max_lag_us = max(self._max_lag_us, lag_us)

    def worst_lag_us(self):
        if self._max_lag_us < 0:
            logging.warning(f"INTERNAL ERROR: _max_lag_us < 0: {self._max_lag_us}")
            self._max_lag_us = 0
        if self._min_lag_us > 0:
            logging.warning(f"INTERNAL ERROR: _min_lag_us > 0: {self._min_lag_us}")
            self._min_lag_us = 0

        # If _min_lag_us < 0, it means that the latency of the first frame is not less than
        # abs(_min_lag_us), and thus we can consider the actual maximum lag to be greater than
        # _max_lag_us by that value.
        return self._max_lag_us - self._min_lag_us


class _BoxPoller:
    def __init__(self, api, box, cpu_cores, period_s):
        self._api = api
        self._box = box
        self._cpu_cores = cpu_cores
        self._period_s = period_s
        self._results = []
        self._stop_event = threading.Event()
        self._exception = None
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
            while not self._stop_event.isSet():
                self._stop_event.wait(self._period_s)
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
            self._exception = e
            self._results.append((None, None, None, None))

    def please_stop(self):
        self._stop_event.set()

    def is_alive(self):
        return self._thread.is_alive()

    def get_issues(self):
        if self._exception is None:
            return []
        if isinstance(self._exception, exceptions.VmsBenchmarkIssue):
            return [self._exception]
        return [exceptions.TestCameraStreamingIssue(
            'Unexpected error during acquiring VMS Server CPU usage, storage, network errors, or swap occupation. '
            'Can be caused by network issues or Server issues.',
            original_exception=self._exception)]

    def get_results(self):
        return self._results.pop()


def _obtain_cameras(test_camera_count, api, box, test_camera_context, ini, conf):
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

    discovering_timeout_seconds = ini['cameraDiscoveryTimeoutSeconds']

    report(
        "    Waiting for virtual camera discovery and going live "
        f"(timeout is {discovering_timeout_seconds} s)..."
    )

    try:
        if ini['addCamerasManually']:
            cameras = api.add_cameras(box.local_ip, test_camera_count)
        else:
            res, cameras = wait_test_cameras_discovered(
                timeout=discovering_timeout_seconds, online_duration=3)
            if not res:
                raise exceptions.TestCameraError('Timeout expired.')
    except Exception as e:
        raise exceptions.TestCameraError(
            f"Not all virtual cameras were discovered or went live.", e) from e

    report("    All virtual cameras discovered and went live.")

    try:
        for camera in cameras:
            if camera.enable_recording(highStreamFps=ini['testStreamFpsHigh']):
                report(f"    Recording on camera {camera.id} enabled.")
            else:
                raise exceptions.TestCameraError(
                    f"Failed enabling recording on camera {camera.id}.")
    except Exception as e:
        raise exceptions.TestCameraError(
            f"Can't enable recording on virtual cameras.", e) from e

    report("    Recording enabled on all virtual cameras.")

    return cameras


def _get_box_time_ms(api):
    host_time_before_s = time.time()
    vms_time_ms = api.get_time()
    request_duration_s = time.time() - host_time_before_s
    return vms_time_ms, request_duration_s


def _run_load_tests(api, box, box_platform, conf, ini, vms):
    report('')
    report('Starting load tests...')
    load_test_started_at_s = time.time()

    swapped_initially_kilobytes = get_cumulative_swap_kilobytes(box)
    if swapped_initially_kilobytes is None:
        report("Cannot obtain swap information.")

    for [test_number, test_camera_count] in zip(itertools.count(1, 1), ini['virtualCameraCount']):
        total_live_stream_count = math.ceil(
            ini['liveStreamsPerCameraRatio'] * test_camera_count)
        total_archive_stream_count = math.ceil(
            ini['archiveStreamsPerCameraRatio'] * test_camera_count)
        report(
            f"\nLoad test #{test_number}: "
            f"{test_camera_count} virtual camera(s), "
            f"{total_live_stream_count} live stream(s), "
            f"{total_archive_stream_count} archive stream(s)...")

        logging.info(f'Starting {test_camera_count} virtual camera(s)...')
        if not ini['enableSecondaryStream']:
            report('INI: Secondary stream is disabled.')
        ini_local_ip = ini['testcameraLocalInterface']
        if ini['testcameraOutputFile']:
            report(f"INI: Going to log testcamera output to {ini['testcameraOutputFile']!r}")
        test_camera_context_manager = test_camera_runner.test_camera_running(
            local_ip=ini_local_ip if ini_local_ip else box.local_ip,
            count=test_camera_count,
            primary_fps=ini['testStreamFpsHigh'],
            secondary_fps=ini['testStreamFpsLow'],
        )
        with test_camera_context_manager as test_camera_context:
            report(f"    Started {test_camera_count} virtual camera(s).")

            cameras = _obtain_cameras(test_camera_count, api, box, test_camera_context, ini, conf)

            report(
                f"    Waiting for the archives to be ready for streaming "
                f"({ini['sleepBeforeCheckingArchiveSeconds']} s)...")
            time.sleep(ini['sleepBeforeCheckingArchiveSeconds'])

            if total_archive_stream_count > 0:
                archive_start_time_ms = api.get_archive_start_time_ms(cameras[0].id)
                archive_read_pos_ms_utc = archive_start_time_ms + ini['archiveReadingPosS'] * 1000
            else:
                archive_read_pos_ms_utc = 0

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

                # TODO: @alevenkov: Move magick number to the INI
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

                streaming_duration_mins = ini['streamingTestDurationMinutes']
                streaming_test_started_at_s = time.time()

                stream_stats = {
                    'live': _StreamTypeStats('live', ini),
                    'archive': _StreamTypeStats('archive', ini),
                }
                streaming_ended_expectedly = False
                issues = []
                first_cpu_times = None
                first_tx_rx_errors = None
                last_tx_rx_errors = None
                cpu_usage_max = None

                box_poller = _BoxPoller(api, box, box_platform.cpu_count, ini['reportingPeriodSeconds'])

                try:
                    first_cycle = True

                    (
                        vms_time_before_test_ms, get_time_request_duration_before_ms
                    ) = _get_box_time_ms(api)
                    host_time_before_test_s = time.time()

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
                            [
                                cpu_times,
                                storage_failure_event_count,
                                tx_rx_errors,
                                swapped_kilobytes,
                            ] = box_poller.get_results()
                        except LookupError:
                            continue

                        if first_cycle is True:
                            first_cpu_times = last_cpu_times = cpu_times
                            first_tx_rx_errors = tx_rx_errors
                            last_tx_rx_errors = first_tx_rx_errors

                            first_cycle = False

                        if cpu_times is not None:
                            cpu_usage_last_minute = cpu_times.cpu_usage(last_cpu_times)
                            if cpu_usage_max is None:
                                cpu_usage_max = cpu_usage_last_minute
                            else:
                                cpu_usage_max = max(cpu_usage_max, cpu_usage_last_minute)
                            last_cpu_times = cpu_times
                        else:
                            cpu_usage_last_minute = None
                        if tx_rx_errors is not None:
                            last_tx_rx_errors = tx_rx_errors

                        live_worst_lag_s = stream_stats['live'].worst_lag_us() / 1_000_000
                        archive_worst_lag_s = stream_stats['archive'].worst_lag_us() / 1_000_000
                        if cpu_usage_last_minute is not None:
                            cpu_usage = f'{round(cpu_usage_last_minute * 100)}%'
                        else:
                            cpu_usage = 'N/A'
                        streaming_test_duration_s = round(time.time() - streaming_test_started_at_s)
                        report(
                            f"    {streaming_test_duration_s} seconds passed; "
                            f"box CPU usage: {cpu_usage}, "
                            f"frame-dropping situations: "
                            f"{stream_stats['live'].frame_drops} (live), "
                            f"{stream_stats['archive'].frame_drops} (archive), "
                            f"stream lag at least: "
                            f"{live_worst_lag_s:.3f} s (live), "
                            f"{archive_worst_lag_s:.3f} s (archive).")

                        if cpu_usage_last_minute is not None:
                            if cpu_usage_last_minute > ini['cpuUsageThreshold']:
                                issues.append(exceptions.CpuUsageThresholdExceededIssue(
                                    cpu_usage_last_minute,
                                    ini['cpuUsageThreshold']
                                ))

                        if storage_failure_event_count is not None:
                            if storage_failure_event_count > 0:
                                issues.append(exceptions.StorageFailuresIssue(storage_failure_event_count))

                        if swapped_initially_kilobytes is not None and swapped_kilobytes is not None:
                            swapped_during_test_kilobytes = swapped_kilobytes - swapped_initially_kilobytes
                            if (
                                ini['enableSwapThreshold'] and
                                swapped_during_test_kilobytes > ini['swapThresholdKilobytes']
                            ):
                                issues.append(exceptions.BoxStateError(
                                    f"More than {ini['swapThresholdKilobytes']} KB swapped at the "
                                    f"box during the tests: {swapped_during_test_kilobytes} KB."))

                        for stream_type in 'live', 'archive':
                            if stream_stats[stream_type].frame_drops > ini['maxAllowedFrameDrops']:
                                issues.append(exceptions.VmsBenchmarkIssue(
                                    f'{stream_stats[stream_type].frame_drops} frame-dropping '
                                    'situation(s) detected '
                                    f'in {stream_type} streams.'))

                            worst_lag_us = stream_stats[stream_type].worst_lag_us()
                            if worst_lag_us > ini['worstAllowedStreamLagUs']:
                                worst_lag_s = worst_lag_us / 1_000_000
                                issues.append(exceptions.TestCameraStreamingIssue(
                                    f'Lag of one of {stream_type} streams '
                                    f'reached {worst_lag_s :.3f} seconds.'
                                ))

                        if issues:
                            streaming_ended_expectedly = True
                            report(
                                f"    Streaming test #{test_number} with {test_camera_count} "
                                "virtual camera(s) aborted because of issues."
                            )
                            break

                        if timestamp_s - streaming_test_started_at_s > streaming_duration_mins * 60:
                            streaming_ended_expectedly = True
                            report(
                                f"    Streaming test #{test_number} with {test_camera_count} "
                                "virtual camera(s) finished."
                            )
                            break

                finally:
                    box_poller.please_stop()

                if last_cpu_times is not first_cpu_times:
                    cpu_usage_avg_report = f'{last_cpu_times.cpu_usage(first_cpu_times) * 100:.0f}%'
                else:
                    cpu_usage_avg_report = 'N/A'

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
                    ram_free_report = f'{to_megabytes(ram_free_bytes)} MB'
                except exceptions.VmsBenchmarkError as e:
                    issues.append(exceptions.UnableToFetchDataFromBox(
                        'Unable to fetch box RAM usage',
                        original_exception=e
                    ))
                    ram_free_report = 'N/A'

                if last_tx_rx_errors is not None and first_tx_rx_errors is not None:
                    tx_rx_errors_during_test_report = last_tx_rx_errors - first_tx_rx_errors
                else:
                    tx_rx_errors_during_test_report = 'N/A'

                if cpu_usage_max is not None:
                    cpu_usage_max_report = f'{cpu_usage_max * 100:.0f}%'
                else:
                    cpu_usage_max_report = 'N/A'

                streaming_test_duration_s = round(time.time() - streaming_test_started_at_s)
                live_frame_drops = stream_stats['live'].frame_drops
                archive_frame_drops = stream_stats['archive'].frame_drops
                report(
                    f"    Streaming test statistics:\n"
                    f"        Frame-dropping situation(s) in live stream: {live_frame_drops} (expected 0)\n"
                    f"        Frame-dropping situation(s) in archive stream: {archive_frame_drops} (expected 0)\n"
                    f"        Box network errors: {tx_rx_errors_during_test_report} (expected 0)\n"
                    f"        Maximum box CPU usage: {cpu_usage_max_report}\n"
                    f"        Average box CPU usage: {cpu_usage_avg_report}\n"
                    f"        Box free RAM: {ram_free_report}\n"
                    f"        Streaming test duration: {streaming_test_duration_s} s"
                )

                if len(issues) > 0:
                    raise exceptions.VmsBenchmarkIssue(f'{len(issues)} issue(s) detected:', sub_issues=issues)

                report(f"    Streaming test #{test_number} with {test_camera_count} virtual camera(s) succeeded.")

    report('\n')

    time.sleep(5)

    try:
        host_time_after_test_s = time.time()
        vms_time_after_test_ms, get_time_request_duration_after_ms = _get_box_time_ms(api)

        logging.info(
            '/api/getTime request duration before the test: '
            f'{get_time_request_duration_before_ms} s.\n'
            '/api/getTime request duration after the test: '
            f'{get_time_request_duration_after_ms} s.\n'
        )

        report(
            'Streaming duration by box clock: '
            f'{(vms_time_after_test_ms - vms_time_before_test_ms) / 1000.0 :.3f} s.\n'
            'Streaming duration by host clock: '
            f'{(host_time_after_test_s - host_time_before_test_s) :.3f} s.\n'
        )
    except Exception as e:
        logging.warning(f"Can't obtain box time: {e}")

    report(
        f'Load tests finished successfully.\n'
        f'Full test duration: {int(time.time() - load_test_started_at_s)} s.'
    )


def _obtain_and_check_box_ram_free_bytes(box_platform, ini, test_camera_count):
    ram_free_bytes = box_platform.obtain_ram_free_bytes()
    ram_required_bytes = test_camera_count * ini['ramPerCameraMegabytes'] * 1024 ** 2

    if ram_free_bytes < ram_required_bytes:
        raise exceptions.InsufficientResourcesError(
            f"Not enough free RAM on the box for {test_camera_count} cameras.")

    return ram_free_bytes


def _remove_cameras(api):
    for i in 1, 2, 3:
        cameras = api.get_test_cameras_all()
        if cameras is not None:
            break
        report(f"Attempt #{i} to get camera list")
        time.sleep(i)

    if cameras is not None:
        if not cameras:
            return

        report('Unregistering all virtual cameras from Server...')
        try:
            for camera in cameras:
                if not api.remove_camera(camera.id):
                    raise exceptions.ServerApiError(
                        message=f"Unable to remove camera with id={camera.id}"
                    )
        except VmsBenchmarkError as e:
            raise exceptions.VmsBenchmarkError(
                f'Unable to unregister virtual cameras.',
                original_exception=e
            )
        report('All virtual cameras unregistered.')
    else:
        raise exceptions.ServerApiError(message="Unable to get camera list.")


def _obtain_box_platform(box, linux_distribution):
    box_platform = BoxPlatform.create(box, linux_distribution)

    def file_system_info_row(storage):
        res = f"        {storage['fs']} on {storage['point']}"

        if 'space_free' in storage and 'space_total' in storage:
            free_gb = int(storage['space_free']) / 1024 ** 3
            total_gb = int(storage['space_total']) / 1024 ** 3
            res += f": free {free_gb:.1f} GB of {total_gb:.1f} GB"

        return res

    file_systems_info = '\n'.join(
        file_system_info_row(storage) for _point, storage in box_platform.storages_list.items()
    )

    report(
        "\nBox properties detected:\n"
        f"    IP address: {box.ip}\n"
        f"    Network adapter name: {box.eth_name}\n"
        f"    Network adapter bandwidth: {(box.eth_speed + ' Mbps') if box.eth_speed else 'Unknown'}\n"
        f"    {box.connection_type_name} user is{'' if box.is_root else ' not'} root.\n"
        f"    Linux distribution name: {linux_distribution.name}\n"
        f"    Linux distribution version: {linux_distribution.version}\n"
        f"    Linux kernel version: {'.'.join(str(c) for c in linux_distribution.kernel_version)}\n"
        f"    Arch: {box_platform.arch}\n"
        f"    Number of CPUs: {box_platform.cpu_count}\n"
        f"    CPU features: {', '.join(box_platform.cpu_features) if len(box_platform.cpu_features) > 0 else 'None'}\n"
        f"    RAM: {to_megabytes(box_platform.ram_bytes)} MB "
        f"({to_megabytes(box_platform.obtain_ram_free_bytes())} MB free)\n"
        f"    File systems: \n{file_systems_info}\n"
    )

    if box_platform.have_storages_list_problems:
        report(
            "WARNING: File system info can be incomplete. "
            f"See details in {log_file_ref}")

    return box_platform


def _check_time_diff(box, ini):
    box_time_output = box.eval('date +%s.%N')
    if not box_time_output:
        raise exceptions.BoxCommandError('Unable to get current box time using the `date` command.')

    # Busybox doesn't support "%N" format specifier in the `date` command, so in this case we have
    # trailing ".%N" in the command output. Strip it and leave seconds-precision value.
    box_time_string = re.sub(r'\.%N$', '', box_time_output.strip())
    try:
        box_time = float(box_time_string)
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
            f"port {'N/A' if vms.port is None else vms.port}, "
            f"pid {'N/A' if vms.pid is None else vms.pid}, "
            f"uid {'N/A' if vms.uid is None else vms.uid})"
            for vms in vmses)
    )
    if len(vmses) > 1:
        raise exceptions.BoxStateError("More than one Server installation found at the box.")
    vms = vmses[0]
    if not vms.is_up():
        raise exceptions.BoxStateError("Server is not currently running at the box.")
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
    vms.override_ini_config({
        'nx_streaming': {
            'enableTimeCorrection': 0,
            'unloopCameraPtsWithModulus': ini['testFileHighPeriodUs'],
        },
    })


def _connect_to_box(conf, ini):
    if conf['boxConnectByTelnet']:
        connection_type, connection_port = BoxConnection.ConnectionType.TELNET, conf['boxTelnetPort']
    else:
        connection_type, connection_port = BoxConnection.ConnectionType.SSH, conf['boxSshPort']

    password = conf.get('boxPassword', None)
    if connection_type == BoxConnection.ConnectionType.SSH and password and platform.system() == 'Linux':
        res = host_tests.SshPassInstalled.call()

        if not res.success:
            raise exceptions.HostPrerequisiteFailed(
                "sshpass is not on PATH"
                " (check if it is installed; to install on Ubuntu: `sudo apt install sshpass`)."
                f"Details for the error: {res.formatted_message()}"
            )

    box = BoxConnection.create_box_connection_object(
        connection_type = connection_type,
        host=conf['boxHostnameOrIp'],
        login=conf['boxLogin'],
        password=password,
        ssh_key=conf['boxSshKey'],
        port=connection_port,
    )
    if connection_type == BoxConnection.ConnectionType.SSH and platform.system() == 'Windows':
        try:
            host_key = service_objects.SshHostKeyObtainer(box, conf).call()
        except exceptions.SshHostKeyObtainingFailed as exception:
            raise exceptions.BoxCommandError(
                f'Unable to connect to the box via ssh; check box credentials in {conf.filepath!r}',
                original_exception=exception
            )
        box.supply_host_key(host_key)

    try:
        start_time_s = time.time()
        box.sh('true', throw_exception_on_error=True)
        command_execution_duration_ms = int((time.time() - start_time_s) * 1000)
        if connection_type == BoxConnection.ConnectionType.TELNET:
            if command_execution_duration_ms > ini['boxTelnetConnectionWarningThresholdMs']:
                report('')
                report(
                    'WARNING: Telnet connection is very slow, it can affect test results. '
                    'Consider fixing Box system settings (see readme.md for more info).')

    except exceptions.BoxCommandError as e:
        raise exceptions.BoxConnectionError(
            (
                "Can't connect to the box: " +
                "check box configuration settings (host, login, password and so on)."
            ),
            original_exception=e,
        )

    box.obtain_connection_info()
    if not box.is_root:
        res = box_tests.SudoConfigured(box).call()

        if not res.success:
            raise exceptions.SshHostKeyObtainingFailed(
                'Sudo is not configured properly, check that user is root or can run `sudo true` '
                'without typing a password.\n'
                f"Details of the error:\n{res.formatted_message()}"
            )
    return box


@click.command(context_settings=dict(help_option_names=["-h", "--help"]))
@click.option(
    '--config',
    '-c',
    'conf_file',
    metavar='<filename>',
    show_default=True,
    help='Configuration file. [default: vms_benchmark.conf]'
)
# This options has the default value, see below (ini_file_default).
@click.option(
    '--ini-config',
    '-i',
    'ini_file',
    metavar='<filename>',
    help=('Internal configuration file for experimenting and debugging.'
        '  [default: vms_benchmark.ini]')
)
@click.option(
    '--log', '-l', 'log_file',
    default='vms_benchmark.log',
    metavar='<filename>',
    show_default=True,
    help='Detailed log of all actions; intended to be studied by the support team.'
)
def main(conf_file, ini_file, log_file):
    global log_file_ref
    log_file_ref = repr(log_file)
    handler = logging.FileHandler(log_file, 'w', 'utf8')
    handler.setFormatter(logging.Formatter('%(asctime)s.%(msecs)03d %(message)s', '%Y-%m-%d %H:%M:%S'))
    logging.getLogger().addHandler(handler)
    logging.getLogger().setLevel(logging.DEBUG)
    report(f"VMS Benchmark (version: {vms_version()}) started; logging to {log_file_ref}.")

    if os.path.exists('./build_info.txt'):
        with open('./build_info.txt', 'r') as f:
            logging.info('build_info.txt\n' + ''.join(f.readlines()))

    conf, ini = load_configs(conf_file, ini_file)

    if ini['liveStreamsPerCameraRatio'] == 0 and ini['archiveStreamsPerCameraRatio'] == 0:
        raise exceptions.ConfigOptionsRestrictionError(
            'Config settings liveStreamsPerCameraRatio and archiveStreamsPerCameraRatio should ' +
            'not be zero simultaneously.'
        )

    # On Windows plink tool is used and this tool doesn't support the fallback to default for the
    # SSH login.
    if platform.system() == 'Windows' and not conf["boxLogin"]:
        class InvalidBoxLoginConfigOptionValue(exceptions.VmsBenchmarkError):
            pass

        raise InvalidBoxLoginConfigOptionValue(
            f"Config option boxLogin should be set and not empty on Windows.")

    box = _connect_to_box(conf, ini)
    linux_distribution = LinuxDistributionDetector.detect(box)
    box_platform = _obtain_box_platform(box, linux_distribution)
    _check_time_diff(box, ini)

    vms = _obtain_running_vms(box, linux_distribution)
    if not ini['unloopViaTestcamera']:
        vms.dismount_ini_dirs()
    try:
        api = ServerApi(box.ip, vms.port, user=conf['vmsUser'], password=conf['vmsPassword'])
        _test_api(api, ini)

        storages = _get_storages(api, ini)
        _stop_vms(vms)
        if not ini['unloopViaTestcamera']:
            _override_ini_config(vms, ini)
        _clear_storages(box, storages, conf)
        vms = _restart_vms(api, box, linux_distribution, vms, ini)

        ram_free_bytes = box_platform.obtain_ram_free_bytes()
        report(
            (f"Box RAM free after restart: {to_megabytes(ram_free_bytes)} MB "
                f"of {to_megabytes(box_platform.ram_bytes)} MB")
        )

        _remove_cameras(api)

        _check_storages(api, ini, camera_count=max(ini['virtualCameraCount']))

        _run_load_tests(api, box, box_platform, conf, ini, vms)

        report('\nSUCCESS: All tests finished.')
    finally:
        if not ini['unloopViaTestcamera']:
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
            timeout_s=conf['archiveDeletingTimeoutSeconds'], su=True, throw_exception_on_error=True)
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
    # TODO: This should not happen, consider removing.
    elif isinstance(exception, urllib.error.HTTPError):
        if exception.code == 401:
            return ('Server refuses passed credentials: ' +
                'check .conf options vmsUser and vmsPassword.')
        else:
            return f'Unexpected HTTP request error (code {exception.code}).'
    elif isinstance(exception, AssertionError):
        return f"Internal error (assertion failed)."
    else:
        return str(exception) or "Exception " + type(exception).__name__


def _do_report_exception(exception, recursive_level, prefix=''):
    indent = '    ' * recursive_level
    if isinstance(exception, VmsBenchmarkError):
        s = str(exception) or "Exception " + type(exception).__name__
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
            report_exception(
                'ISSUE',
                e,
                'Can be caused by network issues, or poor performance of the box or the host.'
            )
        except VmsBenchmarkError as e:
            report_exception(
                'ERROR',
                e,
                f'Technical details may be available in {log_file_ref}.' if log_file_ref else None
            )
        except Exception as e:
            report_exception(
                f'UNEXPECTED ERROR',
                e,
                f'Details may be available in {log_file_ref}.' if log_file_ref else None
            )

        logging.info(f'VMS Benchmark finished with an exception.')
    except Exception as e:
        sys.stderr.write(
            f'INTERNAL ERROR: {e}\n'
            f'\n'
            f'Please send the console output ' +
            (f'and {log_file_ref} ' if log_file_ref else '') + 'to the support team.\n')

    sys.exit(1)
