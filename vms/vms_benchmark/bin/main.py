#!/usr/bin/env python3
import datetime
import itertools
import logging
import math
import platform
import signal
import sys
import time
import uuid
from typing import List, Tuple, Optional

from vms_benchmark.camera import Camera

# This block ensures that ^C interrupts are handled quietly.
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
import click
import threading


def to_megabytes(bytes_count):
    return bytes_count // (1024 * 1024)


def to_percentage(share_0_1):
    return round(share_0_1 * 100)


def load_configs(conf_file, ini_file):
    conf_option_descriptions = {
        "boxHostnameOrIp": {
            "type": 'string',
        },
        "boxLogin": {
            "optional": True,
            "type": 'string',
        },
        "boxPassword": {
            "optional": True,
            "type": 'string',
        },
        "boxSshPort": {
            "optional": True,
            "type": 'integer',
            "range": [0, 65535],
            "default": 22,
        },
        "vmsUser": {
            "optional": True,
            "type": "string",
            "default": "admin",
        },
        "vmsPassword": {
            "optional": True,
            "type": "string",
            "default": "admin",
        },
        "virtualCameraCount": {
            "optional": False,
            "type": "integers",
        },
        "liveStreamsPerCameraRatio": {
            "optional": True,
            "type": 'float',
            "default": 1.0,
        },
        "archiveStreamsPerCameraRatio": {
            "optional": True,
            "type": 'float',
            "default": 0.2,
        },
        "streamingTestDurationMinutes": {
            "optional": True,
            "type": 'integer',
            "default": 4 * 60,
        },
        "cameraDiscoveryTimeoutSeconds": {
            "optional": True,
            "type": 'integer',
            "default": 3 * 60,
        },
    }

    conf = ConfigParser(conf_file, conf_option_descriptions)

    ini_option_descriptions = {
        "testcameraBin": {
            "optional": True,
            "type": 'string',
            "default": './testcamera/testcamera'
        },
        "rtspPerfBin": {
            "optional": True,
            "type": 'string',
            "default": './testcamera/rtsp_perf'
        },
        "testFileHighResolution": {
            "optional": True,
            "type": 'string',
            "default": './high.ts'
        },
        "testFileHighDurationMs": {
            "optional": True,
            "type": 'integer',
            "default": 10000
        },
        "testFileLowResolution": {
            "optional": True,
            "type": 'string',
            "default": './low.ts'
        },
        "testFileFps": {
            "optional": True,
            "type": 'integer',
            "default": 30
        },
        "testStreamFpsHigh": {
            "optional": True,
            "type": 'integer',
            "default": 30
        },
        "testStreamFpsLow": {
            "optional": True,
            "type": 'integer',
            "default": 7
        },
        "testcameraDebug": {
            "optional": True,
            "type": 'boolean',
            "default": False
        },
        "testcameraLocalInterface": {
            "optional": True,
            "type": 'string',
            "default": ''
        },
        "cpuUsageThreshold": {
            "optional": True,
            "type": 'float',
            "default": 0.5
        },
        "archiveBitratePerCameraMbps": {
            "optional": True,
            "type": 'integer',
            "default": 10,
        },
        "minimumArchiveFreeSpacePerCameraSeconds": {
            "optional": True,
            "type": 'integer',
            "default": 180,
        },
        "timeDiffThresholdSeconds": {
            "optional": True,
            "type": 'float',
            "default": 180
        },
        "swapThresholdMegabytes": {
            "optional": True,
            "type": 'integer',
            "default": 0,
        },
        "sleepBeforeCheckingArchiveSeconds": {
            "optional": True,
            "type": 'integer',
            "default": 90,
        },
        "maxAllowedNetworkErrors": {
            "optional": True,
            "type": 'integer',
            "default": 0,
        },
        "ramPerCameraMegabytes": {
            "optional": True,
            "type": 'integer',
            "default": 40,
        },
        "sshCommandTimeoutS": {
            "optional": True,
            "type": 'integer',
            "default": 5,
        },
        "sshServiceCommandTimeoutS": {
            "optional": True,
            "type": 'integer',
            "default": 30,
        },
        "sshGetFileContentTimeoutS": {
            "optional": True,
            "type": 'integer',
            "default": 30,
        },
        "sshGetProcMeminfoTimeoutS": {
            "optional": True,
            "type": 'integer',
            "default": 10,
        },
    }

    ini = ConfigParser(ini_file, ini_option_descriptions, is_file_optional=True)

    test_camera_runner.ini_testcamera_bin = ini['testcameraBin']
    stream_reader_runner.ini_rtsp_perf_bin = ini['rtspPerfBin']
    test_camera_runner.ini_test_file_high_resolution = ini['testFileHighResolution']
    test_camera_runner.ini_test_file_low_resolution = ini['testFileLowResolution']
    test_camera_runner.ini_testcamera_debug = ini['testcameraDebug']
    box_connection.ini_ssh_command_timeout_s = ini['sshCommandTimeoutS']
    box_connection.ini_ssh_get_file_content_timeout_s = ini['sshGetFileContentTimeoutS']
    vms_scanner.ini_ssh_service_command_timeout_s = ini['sshServiceCommandTimeoutS']
    box_platform.ini_ssh_get_proc_meminfo_timeout_s = ini['sshGetProcMeminfoTimeoutS']

    if ini.ORIGINAL_OPTIONS is not None:
        report(f"Overriding default options via {ini_file}:")
        for k, v in ini.ORIGINAL_OPTIONS.items():
            report(f"    {k}={v}")
    report('')

    report(f"Configuration defined in {conf_file}:")
    for k, v in conf.options.items():
        report(f"    {k}={v}")

    return conf, ini


def log_exception(context_name):
    logging.exception(f'Exception yielding {context_name}')


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


def report_server_storage_failures(api, streaming_started_at):
    report(f"    Requesting potential failure events from the Server...")
    log_events = api.get_events(streaming_started_at)
    storage_failure_event_count = sum(
        event['aggregationCount']
        for event in log_events
        if event['eventParams'].get('eventType', None) == 'storageFailureEvent'
    )
    report(f"        Storage failures: {storage_failure_event_count}")

    if storage_failure_event_count > 0:
        raise exceptions.StorageFailuresIssue(storage_failure_event_count)


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


def get_cumulative_swap_bytes(box):
    output = box.eval('cat /proc/vmstat')
    raw = [line.split() for line in output.splitlines()]
    try:
        data = {k: int(v) for k, v in raw}
    except ValueError:
        return None
    try:
        kilobytes_swapped = data['pgpgin']  # pswpin is the number of pages.
    except KeyError:
        return None
    return kilobytes_swapped * 1024


def box_uptime(box):
    uptime_content = box.eval('cat /proc/uptime')

    if uptime_content is None:
        raise exceptions.BoxFileContentError('/proc/uptime')

    uptime_components = uptime_content.split()
    uptime_s, idle_time_s = [float(v) for v in uptime_components[0:2]]

    return uptime_s, idle_time_s


def box_tx_rx_errors(box):
    errors_content = box.eval(f'cat /sys/class/net/{box.eth_name}/statistics/tx_errors /sys/class/net/{box.eth_name}/statistics/rx_errors')

    if errors_content is None:
        raise exceptions.BoxFileContentError(f'/sys/class/net/{box.eth_name}/statistics/{{tx,rx}}_errors')

    errors_components = errors_content.split('\n')
    tx_errors, rx_errors = [int(v) for v in errors_components[0:2]]

    return tx_errors + rx_errors


def _print(message):
    print(message, flush=True)


def report(message):
    _print(message)
    if message.strip():
        logging.info(message.strip())


# Refers to the log file in the messages to the user. Filled after determining the log file path.
log_file_ref = None


def _test_api(api):
    logging.info('Starting REST API basic test...')

    def wait_for_api(timeout=60):
        started_at = time.time()

        while True:
            resp = api.ping()

            if resp and resp.code == 200 and resp.payload.get('error', None) == '0':
                break

            if time.time() - started_at > timeout:
                return False

            time.sleep(0.5)

        time.sleep(5)
        return True

    if not wait_for_api():
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
                'Unable to get Server Storages via REST HTTP: request failed',
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


def _run_load_test(api, box, box_platform, conf, ini, vms):
    report('')
    report('Starting load test...')
    load_test_started_at_s = time.time()

    swapped_before_bytes = get_cumulative_swap_bytes(box)
    if swapped_before_bytes is None:
        report("Cannot obtain swap information.")

    for [test_number, test_camera_count] in zip(itertools.count(1, 1), conf['virtualCameraCount']):
        ram_available_bytes = box_platform.ram_available_bytes()
        ram_free_bytes = ram_available_bytes if ram_available_bytes else box_platform.ram_free_bytes()

        ram_required_bytes = test_camera_count * ini['ramPerCameraMegabytes'] * 1024 * 1024
        if ram_available_bytes and ram_available_bytes < ram_required_bytes:
            raise exceptions.InsufficientResourcesError(
                f"Not enough free RAM on the box for {test_camera_count} cameras.")

        total_live_stream_count = math.ceil(conf['liveStreamsPerCameraRatio'] * test_camera_count)
        total_archive_stream_count = math.ceil(conf['archiveStreamsPerCameraRatio'] * test_camera_count)
        report(
            f"Load test #{test_number}: "
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
        frame_drops_sum = 0
        max_lag_s = 0
        with test_camera_context_manager as test_camera_context:
            report(f"    Started {test_camera_count} virtual camera(s).")

            def wait_test_cameras_discovered(timeout, online_duration) -> Tuple[bool, Optional[List[Camera]]]:
                started_at = time.time()
                detection_started_at = None
                while time.time() - started_at < timeout:
                    if test_camera_context.poll() is not None:
                        raise exceptions.TestCameraError(
                            f'Test Camera process exited unexpectedly with code {test_camera_context.returncode}')

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
                res, cameras = wait_test_cameras_discovered(timeout=discovering_timeout_seconds, online_duration=3)
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
                raise exceptions.TestCameraError(f"Not all virtual cameras were discovered or went live.", e) from e

            report(
                f"    Waiting for the archives to be ready for streaming "
                f"({ini['sleepBeforeCheckingArchiveSeconds']} s)...")
            time.sleep(ini['sleepBeforeCheckingArchiveSeconds'])

            report('    Streaming test...')

            stream_reader_context_manager = stream_reader_runner.stream_reader_running(
                camera_ids=[camera.id for camera in cameras],
                total_live_stream_count=total_live_stream_count,
                total_archive_stream_count=total_archive_stream_count,
                user=conf['vmsUser'],
                password=conf['vmsPassword'],
                box_ip=box.ip,
                vms_port=vms.port,
            )
            with stream_reader_context_manager as stream_reader_context:
                stream_reader_process = stream_reader_context[0]
                streams = stream_reader_context[1]

                started = False
                stream_opening_started_at_s = time.time()

                streams_started_flags = dict((stream_id, False) for stream_id, _ in streams.items())

                while time.time() - stream_opening_started_at_s < 25:
                    if stream_reader_process.poll() is not None:
                        raise exceptions.RtspPerfError("Can't open streams or streaming unexpectedly ended.")

                    line = stream_reader_process.stdout.readline().decode('UTF-8')
                    warning_prefix = 'WARNING: '
                    if line.startswith(warning_prefix):
                        raise exceptions.RtspPerfError("Streaming error: " + line[len(warning_prefix):])

                    import re
                    match_res = re.match(r'.*\/([a-z0-9-]+)\?(.*) timestamp (\d+) us', line.strip())
                    if not match_res:
                        continue

                    rtsp_url_params = dict(
                        [param_pair[0], (param_pair[1] if len(param_pair) > 1 else None)]
                        for param_pair in [
                            param_pair_str.split('=')
                            for param_pair_str in match_res.group(2).split('&')
                        ]
                    )

                    stream_id = rtsp_url_params['vms_benchmark_stream_id']

                    if stream_id not in streams_started_flags:
                        raise exceptions.RtspPerfError(f'Cannot open video streams: unexpected stream id.')

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

                last_ptses = {}
                first_timestamps_s = {}
                frames = {}
                frame_drops = {}
                lags = {}
                streaming_ended_expectedly = False
                issues = []
                cpu_usage_max_collector = [None]
                cpu_usage_avg_collector = []
                tx_rx_errors_collector = [None]

                box_poller_thread_stop_event = threading.Event()
                box_poller_thread_exceptions_collector = []

                def box_poller(
                        stop_event,
                        exception_collector,
                        cpu_usage_max_collector,
                        cpu_usage_avg_collector,
                        tx_rx_errors_collector
                ):
                    prev_uptime, prev_idle_time_s = None, None
                    try:
                        while not stop_event.isSet():
                            uptime, idle_time_s = box_uptime(box)
                            if prev_idle_time_s is not None and prev_uptime is not None:
                                idle_time_delta_total_s = idle_time_s - prev_idle_time_s
                                idle_time_delta_per_core_s = idle_time_delta_total_s / box_platform.cpu_count
                                uptime_delta_s = uptime - prev_uptime
                                cpu_usage_last_minute = 1.0 - idle_time_delta_per_core_s / uptime_delta_s
                                report(
                                    f"    {round(time.time() - streaming_test_started_at_s)} seconds passed; "
                                    f"box CPU usage: {round(cpu_usage_last_minute * 100)}%, "
                                    f"dropped frames: {frame_drops_sum}, "
                                    f"max stream lag: {max_lag_s:.3f} s.")
                                if not cpu_usage_max_collector[0] is None:
                                    cpu_usage_max_collector[0] = max(cpu_usage_max_collector[0], cpu_usage_last_minute)
                                else:
                                    cpu_usage_max_collector[0] = cpu_usage_last_minute
                                cpu_usage_avg_collector.append(cpu_usage_last_minute)
                                if cpu_usage_last_minute > ini['cpuUsageThreshold']:
                                    raise exceptions.CpuUsageThresholdExceededIssue(
                                        cpu_usage_last_minute,
                                        ini['cpuUsageThreshold']
                                    )
                            prev_uptime, prev_idle_time_s = uptime, idle_time_s

                            tx_rx_errors = box_tx_rx_errors(box)

                            if tx_rx_errors_collector[0] is None:
                                tx_rx_errors_collector[0] = 0

                            tx_rx_errors_collector[0] += tx_rx_errors

                            stop_event.wait(60)
                    except Exception as e:
                        exception_collector.append(e)
                        return

                box_poller_thread = threading.Thread(
                    target=box_poller,
                    args=(
                        box_poller_thread_stop_event,
                        box_poller_thread_exceptions_collector,
                        cpu_usage_max_collector,
                        cpu_usage_avg_collector,
                        tx_rx_errors_collector,
                    )
                )

                box_poller_thread.start()

                try:
                    while time.time() - streaming_test_started_at_s < streaming_duration_mins * 60:
                        if stream_reader_process.poll() is not None:
                            raise exceptions.RtspPerfError("Streaming unexpectedly ended.")

                        if not box_poller_thread.is_alive():
                            if (
                                len(box_poller_thread_exceptions_collector) > 0 and
                                    isinstance(
                                        box_poller_thread_exceptions_collector[0], exceptions.VmsBenchmarkIssue
                                    )
                            ):
                                issues.append(box_poller_thread_exceptions_collector[0])
                            else:
                                issues.append(
                                    exceptions.TestCameraStreamingIssue(
                                        (
                                            'Unexpected error during acquiring VMS Server CPU usage. ' +
                                            'Can be caused by network issues or Server issues.'
                                        ),
                                        original_exception=box_poller_thread_exceptions_collector
                                    )
                                )
                            streaming_ended_expectedly = True
                            break

                        line = stream_reader_process.stdout.readline().decode('UTF-8')

                        match_res = re.match(r'.*\/([a-z0-9-]+)\?(.*) timestamp (\d+) us', line.strip())
                        if not match_res:
                            continue

                        rtsp_url_params = dict(
                            [param_pair[0], (param_pair[1] if len(param_pair) > 1 else None)]
                            for param_pair in [
                                param_pair_str.split('=')
                                for param_pair_str in match_res.group(2).split('&')
                            ]
                        )
                        pts = int(match_res.group(3))

                        pts_stream_id = rtsp_url_params['vms_benchmark_stream_id']

                        frames[pts_stream_id] = frames.get(pts_stream_id, 0) + 1

                        timestamp_s = time.time()

                        if pts_stream_id not in first_timestamps_s:
                            first_timestamps_s[pts_stream_id] = timestamp_s
                        elif streams[pts_stream_id]['type'] == 'live':
                            frame_interval_s = 1.0 / float(ini['testFileFps'])
                            this_frame_offset_s = frames.get(pts_stream_id, 0) * frame_interval_s
                            expected_frame_time_s = first_timestamps_s[pts_stream_id] + this_frame_offset_s
                            this_frame_lag_s = timestamp_s - expected_frame_time_s
                            lags[pts_stream_id] = max(lags.get(pts_stream_id, 0), this_frame_lag_s)
                            max_lag_s = max(max_lag_s, this_frame_lag_s)

                        pts_diff_deviation_factor_max = 0.03
                        frame_interval_us = 1000000.0 / float(ini['testFileFps'])
                        pts_diff_expected = frame_interval_us
                        pts_diff = (pts - last_ptses[pts_stream_id]) if pts_stream_id in last_ptses else None
                        pts_diff_max = frame_interval_us * (1.0 + pts_diff_deviation_factor_max)

                        # The value is negative because the first PTS of new loop is less than last PTS of the previous
                        # loop.
                        pts_diff_expected_new_loop = -float(ini['testFileHighDurationMs'])

                        if pts_diff is not None:
                            pts_diff_deviation_factor = lambda diff_expected: abs(
                                (diff_expected - pts_diff) / diff_expected
                            )

                            if (
                                pts_diff_deviation_factor(pts_diff_expected) > pts_diff_deviation_factor_max and
                                pts_diff_deviation_factor(pts_diff_expected_new_loop) > pts_diff_deviation_factor_max
                            ):
                                frame_drops[pts_stream_id] = frame_drops.get(pts_stream_id, 0) + 1
                                frame_drops_sum += 1
                                logging.info(
                                    f'Detected frame drop on camera {streams[pts_stream_id]["camera_id"]}: '
                                    f'diff {pts_diff} us (max {math.ceil(pts_diff_max)} us), '
                                    f'PTS {pts} us, now {math.ceil(timestamp_s * 1000000)} us'
                                )

                        if timestamp_s - streaming_test_started_at_s > streaming_duration_mins * 60:
                            streaming_ended_expectedly = True
                            report(
                                f"    Streaming test #{test_number} with {test_camera_count} virtual camera(s) finished.")
                            break

                        last_ptses[pts_stream_id] = pts
                finally:
                    box_poller_thread_stop_event.set()

                cpu_usage_max = cpu_usage_max_collector[0]

                if cpu_usage_avg_collector:
                    cpu_usage_avg = sum(cpu_usage_avg_collector) / len(cpu_usage_avg_collector)
                else:
                    cpu_usage_avg = None

                if not streaming_ended_expectedly:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        'the stream has unexpectedly finished; ' +
                        'can be caused by network issues or Server issues.',
                        original_exception=issues
                    )

                if time.time() - timestamp_s > 5:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        'the stream has hung; ' +
                        'can be caused by network issues or Server issues.')

                try:
                    ram_available_bytes = box_platform.ram_available_bytes()
                    ram_free_bytes = ram_available_bytes if ram_available_bytes else box_platform.ram_free_bytes()
                except exceptions.VmsBenchmarkError as e:
                    issues.append(exceptions.UnableToFetchDataFromBox(
                        'Unable to fetch box RAM usage',
                        original_exception=e
                    ))

                if tx_rx_errors_collector[0] is None:
                    tx_rx_errors_collector[0] = 0

                if tx_rx_errors_collector[0] > ini['maxAllowedNetworkErrors']:
                    issues.append(exceptions.TestCameraStreamingIssue(
                        f"Network errors detected: {tx_rx_errors_collector[0]}"
                    ))

                streaming_test_duration_s = round(time.time() - streaming_test_started_at_s)
                report(
                    f"    Streaming test statistics:\n"
                    f"        Frame drops: {sum(frame_drops.values())} (expected 0)\n"
                    + (
                        f"        Box network errors: {tx_rx_errors_collector[0]} (expected 0)\n"
                        if tx_rx_errors_collector[0] is not None else '')
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

                if frame_drops_sum > 0:
                    issues.append(exceptions.VmsBenchmarkIssue(f'{frame_drops_sum} frame drops detected.'))

                try:
                    report_server_storage_failures(api, round(streaming_test_started_at_s * 1000))
                except exceptions.VmsBenchmarkIssue as e:
                    issues.append(e)

                max_allowed_lag_seconds = 5
                if max_lag_s > max_allowed_lag_seconds:
                    issues.append(exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        f'the video lag {max_lag_s:.3f} seconds is more than {max_allowed_lag_seconds} seconds.'
                    ))

                if len(issues) > 0:
                    raise exceptions.VmsBenchmarkIssue(f'{len(issues)} issue(s) detected:', sub_issues=issues)

                report(f"    Streaming test #{test_number} with {test_camera_count} virtual camera(s) succeeded.")

    if swapped_before_bytes is not None:
        swapped_after_bytes = get_cumulative_swap_bytes(box)
        swapped_during_test_bytes = swapped_after_bytes - swapped_before_bytes
        if swapped_during_test_bytes > ini['swapThresholdMegabytes'] * 1024 * 1024:
            if swapped_during_test_bytes > 1024 * 1024:
                swapped_str = f'{swapped_during_test_bytes // (1024 * 1024)} MB'
            else:
                swapped_str = f'{swapped_during_test_bytes} bytes'
            raise exceptions.BoxStateError(
                f"More than {ini['swapThresholdMegabytes']} MB swapped at the box during the tests: {swapped_str}.")

    report(f'Load test finished successfully; duration: {int(time.time() - load_test_started_at_s)} s.')


def _test_vms(api, box, box_platform, conf, ini, vms):
    ram_free_bytes = box_platform.ram_available_bytes()
    if ram_free_bytes is None:
        ram_free_bytes = box_platform.ram_free_bytes()
    report(f"Box RAM free: {to_megabytes(ram_free_bytes)} MB of {to_megabytes(box_platform.ram_bytes)} MB")
    _check_storages(api, ini, camera_count=max(conf['virtualCameraCount']))
    for i in 1, 2, 3:
        cameras = api.get_test_cameras_all()
        if cameras is not None:
            break
        report(f"Attempt #{i}to get camera list")
        time.sleep(i)
    if cameras is not None:
        for camera in cameras:
            if not api.remove_camera(camera.id):
                raise exceptions.ServerApiError(
                    message=f"Unable to remove camera with id={camera.id}"
                )
    else:
        raise exceptions.ServerApiError(message="Unable to get camera list.")
    _run_load_test(api, box, box_platform, conf, ini, vms)


def _obtain_box_platform(box, linux_distribution):
    box_platform = BoxPlatform.gather(box, linux_distribution)

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
        f"    RAM: {to_megabytes(box_platform.ram_bytes)} MB ({to_megabytes(box_platform.ram_free_bytes())} MB free)\n"
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

    time.sleep(5)  # TODO: Investigate this sleep

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
                "sshpass is not on PATH" +
                " (check if it is installed; to install on Ubuntu: `sudo apt install sshpass`)." +
                f"Details for the error: {res.formatted_message()}"
            )
    box = BoxConnection(
        host=conf['boxHostnameOrIp'],
        login=conf.get('boxLogin', None),
        password=password,
        port=conf['boxSshPort'],
        conf_file=conf_file
    )
    box.host_key = box_tests.SshHostKeyIsKnown(box, conf_file).call()
    box.obtain_connection_info()
    if not box.is_root:
        res = box_tests.SudoConfigured(box).call()

        if not res.success:
            raise exceptions.SshHostKeyObtainingFailed(
                'Sudo is not configured properly, check that user is root or can run `sudo true` ' +
                'without typing a password.\n' +
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
    report(f"VMS Benchmark started; logging to {log_file_ref}.\n")

    conf, ini = load_configs(conf_file, ini_file)

    box = _connect_to_box(conf, conf_file)
    linux_distribution = LinuxDistributionDetector.detect(box)
    box_platform = _obtain_box_platform(box, linux_distribution)
    _check_time_diff(box, ini)

    vms = _obtain_running_vms(box, linux_distribution)
    vms.dismount_ini_dirs()
    try:
        api = ServerApi(box.ip, vms.port, user=conf['vmsUser'], password=conf['vmsPassword'])
        _test_api(api)

        storages = _get_storages(api)
        _stop_vms(vms)
        _override_ini_config(vms, ini)
        _clear_storages(box, storages)
        vms = _restart_vms(box, linux_distribution, vms)

        _test_vms(api, box, box_platform, conf, ini, vms)

        report('\nSUCCESS: All tests finished.')
    finally:
        vms.dismount_ini_dirs()

    return 0


def _restart_vms(box, linux_distribution, vms):
    report('Starting Server...')
    vms.start(exc=True)
    vms = _obtain_restarted_vms(box, linux_distribution)
    report('Server started.')
    return vms


def _stop_vms(vms):
    report('Stopping server...')
    vms.stop(exc=True)
    report('Server stopped.')


def _clear_storages(box, storages: List[Storage]):
    for storage in storages:
        box.sh(
            f"rm -rf "
            f"'{storage.url}/hi_quality' "
            f"'{storage.url}/low_quality' "
            f"'{storage.url}/'*_media.nxdb",
            su=True, exc=True)
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


def nx_format_exception(exception):
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


def nx_print_exception(context_name, exception, recursive_level=0):
    string_indent = '  ' * recursive_level
    prefix = (context_name + ': ') if context_name else ''
    if isinstance(exception, exceptions.VmsBenchmarkError):
        _print(f"{string_indent}{prefix}{str(exception)}")
        if isinstance(exception, exceptions.VmsBenchmarkIssue):
            for e in exception.sub_issues:
                nx_print_exception('', e, recursive_level=recursive_level + 2)
        if exception.original_exception:
            _print(f'{string_indent}Caused by:')
            if isinstance(exception.original_exception, list):
                for e in exception.original_exception:
                    nx_print_exception('', e, recursive_level=recursive_level + 2)
            else:
                nx_print_exception('', exception.original_exception,
                    recursive_level=recursive_level + 2)
    else:
        _print(
            f'{string_indent}{prefix}{nx_format_exception(exception)}'
            if recursive_level > 0
            else f'{string_indent}ERROR: {nx_format_exception(exception)}'
        )


if __name__ == '__main__':
    try:
        try:
            sys.exit(main())
        except (exceptions.VmsBenchmarkIssue, urllib.error.HTTPError) as e:
            nx_print_exception('ISSUE', e)
            _print('\nNOTE: Can be caused by network issues, or poor performance of the box or the host.')
            log_exception('ISSUE')
        except exceptions.VmsBenchmarkError as e:
            nx_print_exception('ERROR', e)
            if log_file_ref:
                _print(f'\nNOTE: Technical details may be available in {log_file_ref}.')
            log_exception('ERROR')
        except Exception as e:
            _print(f'UNEXPECTED ERROR: {e}')
            if log_file_ref:
                _print(f'\nNOTE: Details may be available in {log_file_ref}.')
            log_exception('UNEXPECTED ERROR')
        finally:
            logging.info(f'VMS Benchmark finished at {datetime.datetime.now():%Y-%m-%d %H:%M:%S}.')
    except Exception as e:
        _print(f'INTERNAL ERROR: {e}')
        _print(f'\nPlease send the complete output ' +
            (f'and {log_file_ref} ' if log_file_ref else '') + 'to the support team.')

    sys.exit(1)
