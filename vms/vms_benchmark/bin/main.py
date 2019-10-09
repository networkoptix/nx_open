#!/usr/bin/env python3

import datetime
import platform
import signal
import sys
import time
import os
import logging

logging.basicConfig(filename='vms_benchmark.log', filemode='w', level=logging.DEBUG)

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
        debug_signum = signal.SIGUSR2 # bug #424259
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
from vms_benchmark import test_camera_runner
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
            "range": [0, 31768],
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
        "virtualCamerasCount": {
            "optional": False,
            "type": "integers",
        },
        "streamsPerTestCamera": {
            "optional": True,
            "type": 'integer',
            "default": 1,
        },
        "streamingTestDurationMinutes": {
            "optional": True,
            "type": 'integer',
            "default": 1 if os.path.exists('.not_installed') else 4 * 60,
        },
        "cameraDiscoveryTimeoutSeconds": {
            "optional": True,
            "type": 'integer',
            "default": 180,
        },
    }

    conf = ConfigParser(conf_file, conf_option_descriptions)

    ini_option_descriptions = {
        "testCameraBin": {
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
        "testCameraDebug": {
            "optional": True,
            "type": 'boolean',
            "default": False
        },
        "cpuUsageThreshold": {
            "optional": True,
            "type": 'float',
            "default": 0.5
        },
        "minimumStorageFreeBytes": {
            "optional": True,
            "type": 'integer',
            "default": 10 * 1024 * 1024
        },
    }

    ini = ConfigParser(ini_file, ini_option_descriptions, is_file_optional=True)

    if ini['testCameraBin']:
        test_camera_runner.binary_file = ini['testCameraBin']
    if ini['rtspPerfBin']:
        stream_reader_runner.binary_file = ini['rtspPerfBin']
    if ini['testFileHighResolution']:
        test_camera_runner.test_file_high_resolution = ini['testFileHighResolution']
    if ini['testFileLowResolution']:
        test_camera_runner.test_file_low_resolution = ini['testFileLowResolution']
    test_camera_runner.debug = ini['testCameraDebug']

    return conf, ini


def log_exception(context_name, exception):
    logging.exception(f'Exception: {context_name}: {type(exception)}: {str(exception)}')


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
    print(f"    Requesting potential failure events from the Server...")
    log_events = api.get_events(streaming_started_at)
    storage_failure_events_count = sum(
        event['aggregationCount']
        for event in log_events
        if event['eventParams'].get('eventType', None) == 'storageFailureEvent'
    )
    print(f"        Storage failures: {storage_failure_events_count}")

    if storage_failure_events_count > 0:
        raise exceptions.StorageFailuresIssue(storage_failure_events_count)


def get_writable_storages(api, ini):
    try:
        storages = api.get_storage_spaces()
    except exceptions.VmsBenchmarkError as e:
        raise exceptions.ServerApiError('Unable to get VMS storages via REST HTTP', original_exception=e)

    if storages is None:
        raise exceptions.ServerApiError('Unable to get VMS storages via REST HTTP: request failed')

    def storage_is_writable(storage):
        free_space = int(storage['freeSpace'])
        reserved_space = int(storage['reservedSpace'])

        return free_space >= reserved_space + ini['minimumStorageFreeBytes']

    try:
        result = [storage for storage in storages if storage_is_writable(storage)]
    except (ValueError, KeyError) as e:
        raise exceptions.ServerApiResponseError("Bad response body for storage info request", original_exception=e)

    return result


def get_cumulative_swap_bytes(box):
    output = box.eval('cat /proc/vmstat')
    raw = [line.split() for line in output.splitlines()]
    data = {k: int(v) for k, v in raw}
    kilobytes_swapped = data['pgpgin']  # pswpin is the number of pages.
    return kilobytes_swapped * 1024


@click.command()
@click.option('--config', 'conf_file', default='vms_benchmark.conf', help='Configuration file.')
@click.option('-C', 'conf_file', default='vms_benchmark.conf', help='Configuration file.')
@click.option('--ini-config', 'ini_file', default='vms_benchmark.ini',
    help='Internal configuration file for experimenting and debugging.')
def main(conf_file, ini_file):
    conf, ini = load_configs(conf_file, ini_file)

    if ini.ORIGINAL_OPTIONS is not None:
        print(f"Overriding default options via '{ini_file}':")
        for k, v in ini.ORIGINAL_OPTIONS.items():
            print(f"    {k}={v}")

    password = conf.get('boxPassword', None)

    if password and platform.system() == 'Linux':
        res = host_tests.SshPassInstalled().call()

        if not res.success:
            raise exceptions.HostPrerequisiteFailed(
                "ERROR: sshpass is not on PATH" +
                " (check if it is installed; to install on Ubuntu: `sudo apt install sshpass`)." +
                f"Details for the error: {res.formatted_message()}"
            )

    box = BoxConnection(
        host=conf['boxHostnameOrIp'],
        login=conf.get('boxLogin', None),
        password=password,
        port=conf['boxSshPort']
    )

    res = box_tests.SshHostKeyIsKnown(box).call()

    if not res.success:
        raise exceptions.SshHostKeyObtainingFailed(
            f"Can't obtain ssh host key of the box." +
            (f"\n    Details: {res.formatted_message()}" if res.formatted_message() else "")
        )
    else:
        box.host_key = res.details[0]
        box.obtain_connection_info()

    if not box.is_root:
        res = box_tests.SudoConfigured(box).call()

        if not res.success:
            raise exceptions.SshHostKeyObtainingFailed(
                'Sudo is not configured properly, check that user is root or can run `sudo true` ' +
                'without typing a password.\n' +
                f"Details of the error: {res.formatted_message()}"
            )

    print(f"Box IP: {box.ip}")
    if box.is_root:
        print(f"Box ssh user is root.")

    linux_distribution = LinuxDistributionDetector.detect(box)

    print(f"Linux distribution name: {linux_distribution.name}")
    print(f"Linux distribution version: {linux_distribution.version}")
    print(f"Linux kernel version: {'.'.join(str(c) for c in linux_distribution.kernel_version)}")

    box_platform = BoxPlatform.gather(box, linux_distribution)

    print(f"Arch: {box_platform.arch}")
    print(f"Number of CPUs: {box_platform.cpu_count}")
    print(f"CPU features: {', '.join(box_platform.cpu_features) if len(box_platform.cpu_features) > 0 else '-'}")
    print(f"RAM: {to_megabytes(box_platform.ram_bytes)} MB ({to_megabytes(box_platform.ram_free_bytes())} MB free)")
    print("Volumes:")
    [
        print(f"    {storage['fs']} on {storage['point']}: free {storage['space_free']} of {storage['space_total']}")
        for (point, storage) in
        box_platform.storages_list.items()
    ]

    box_time_output = box.eval('date +%s.%N')
    box_time = float(box_time_output.strip())
    host_time = time.time()
    time_diff_threshold = 3 * 60
    if abs(box_time - host_time) > time_diff_threshold:
        raise exceptions.BoxStateError(f"Box time differs from host time by more than {time_diff_threshold} sec")

    vmses = VmsScanner.scan(box, linux_distribution)

    if vmses and len(vmses) > 0:
        print(f"Detected VMS installation(s):")
        for vms in vmses:
            print(f"    {vms.customization} in {vms.dir} (port {vms.port},", end='')
            print(f" PID {vms.pid if vms.pid else '-'})", end='')
            vms_uid = vms.uid()
            if vms_uid:
                print(f" UID {vms_uid})", end='')
            print('')
    else:
        print("No VMS installations found on the box.")
        print("Nothing to do.")
        return 0

    if len(vmses) > 1:
        raise exceptions.BoxStateError("More than one customizations found.")

    vms = vmses[0]

    if not vms.is_up():
        raise exceptions.BoxStateError("VMS is not running currently.")

    if not vms.override_ini_config(
        {
            'nx_streaming': {
                'enableTimeCorrection': 0,
                'unloopCameraPtsWithModulus': ini['testFileHighDurationMs']*1000 + 1000000//ini['testStreamFpsHigh'],
            },
        }
    ):
        print("Unable to override VMS internal configs.")

    api = ServerApi(box.ip, vms.port, user=conf['vmsUser'], password=conf['vmsPassword'])

    for i in 1, 2, 3:
        cameras = api.get_test_cameras_all()
        if cameras is not None:
            break
        print(f"Attempt #{i}to get camera list")
        time.sleep(i)

    if cameras is None:
        raise exceptions.ServerApiError(message="Unable to get camera list.")

    module_information = api.get_module_information()

    if module_information is None:
        raise exceptions.ServerApiError(message="Unable to get module information.")

    vms_id_raw = module_information.get('id', '{00000000-0000-0000-0000-000000000000}')
    vms_id = vms_id_raw[1:-1] if vms_id_raw[0] == '{' and vms_id_raw[-1] == '}' else vms_id_raw

    vms.stop(exc=True)

    print('Server stopped.')

    for camera in cameras:
        box.sh(f"rm -rf '{vms.dir}/var/data/hi_quality/{camera.mac}'", su=True, exc=True)
        box.sh(f"rm -rf '{vms.dir}/var/data/low_quality/{camera.mac}'", su=True, exc=True)
        box.sh(f"rm -f '{vms.dir}/var/data/{vms_id}_media.nxdb'", su=True, exc=True)

    print('Camera archives cleaned.')

    swapped_before = get_cumulative_swap_bytes(box)

    vms.start(exc=True)

    print('Server starting...')

    def wait_for_server_up(timeout=30):
        started_at = time.time()

        while True:
            global vmses

            vmses = VmsScanner.scan(box, linux_distribution)

            if vmses and len(vmses) > 0 and vmses[0].is_up():
                break

            if time.time() - started_at > timeout:
                return False

            time.sleep(0.5)

        time.sleep(5)  # TODO: Investigate this sleep

        return True

    if not wait_for_server_up():
        raise exceptions.ServerError("Unable to restart Server: Server was not upped.")

    vms = vmses[0]

    print('Server started successfully.')

    ram_free_bytes = box_platform.ram_available_bytes()
    if ram_free_bytes is None:
        ram_free_bytes = box_platform.ram_free_bytes()
    print(f"RAM free: {to_megabytes(ram_free_bytes)} MB of {to_megabytes(box_platform.ram_bytes)} MB")

    api = ServerApi(box.ip, vms.port, user=conf['vmsUser'], password=conf['vmsPassword'])

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

    print('REST API basic test is OK.')

    api.check_authentication()
    print('REST API authentication test is OK.')

    try:
        storages = get_writable_storages(api, ini)
    except Exception as e:
        raise exceptions.ServerApiError(message="Unable to get VMS storages via REST HTTP", original_exception=e)

    if len(storages) == 0:
        raise exceptions.BoxStateError("Server has no suitable video archive storage, check the free space")

    for i in 1, 2, 3:
        cameras = api.get_test_cameras_all()
        if cameras is not None:
            break
        print(f"Attempt #{i}to get camera list")
        time.sleep(i)

    if cameras is not None:
        for camera in cameras:
            if not api.remove_camera(camera.id):
                raise exceptions.ServerApiError(
                    message=f"Unable to remove camera with id={camera.id}"
                )
    else:
        raise exceptions.ServerApiError(message="Unable to get camera list.")

    # TODO: Move this to internal config.
    ram_bytes_per_camera_by_arch = {
        "armv7l": 100,
        "aarch64": 200,
        "x86_64": 200
    }

    for test_cameras_count in conf['virtualCamerasCount']:
        ram_available_bytes = box_platform.ram_available_bytes()
        ram_free_bytes = ram_available_bytes if ram_available_bytes else box_platform.ram_free_bytes()

        if ram_available_bytes and ram_available_bytes < (
                test_cameras_count * ram_bytes_per_camera_by_arch.get(box_platform.arch, 200) * 1024 * 1024
        ):
            raise exceptions.InsuficientResourcesError(
                f"Not enough free RAM on the box for {test_cameras_count} cameras.")

        print(f"Serving {test_cameras_count} virtual cameras.")
        print("")

        test_camera_context_manager = test_camera_runner.test_camera_running(
            local_ip=box.local_ip,
            count=test_cameras_count,
            primary_fps=ini['testStreamFpsHigh'],
            secondary_fps=ini['testStreamFpsLow'],
        )
        with test_camera_context_manager as test_camera_context:
            print(f"    Started {test_cameras_count} virtual cameras.")

            def wait_test_cameras_discovered(timeout, online_duration):
                started_at = time.time()
                detection_started_at = None
                while time.time() - started_at < timeout:
                    if test_camera_context.poll() is not None:
                        raise exceptions.TestCameraError(
                            f'Test Camera process exited unexpectedly with code {test_camera_context.returncode}')

                    cameras = api.get_test_cameras()

                    if len(cameras) >= test_cameras_count:
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

                print(
                    ("    Waiting for virtual cameras discovery and going live " +
                        f"(timeout is {discovering_timeout_seconds} sec).")
                )
                res, cameras = wait_test_cameras_discovered(timeout=discovering_timeout_seconds, online_duration=3)
                if not res:
                    raise exceptions.TestCameraError('Timeout expired.')

                print("    All virtual cameras discovered and went live.")

                time.sleep(test_cameras_count * 2)

                for camera in cameras:
                    if camera.enable_recording(highStreamFps=ini['testStreamFpsHigh']):
                        print(f"    Recording on camera {camera.id} enabled.")
                    else:
                        raise exceptions.TestCameraError(
                            f"Failed enabling recording on camera {camera.id}.")
            except Exception as e:
                log_exception('Discovering cameras', e)
                raise exceptions.TestCameraError(f"Not all virtual cameras were discovered or went live.", e) from e

            stream_reader_context_manager = stream_reader_runner.stream_reader_running(
                camera_ids=(camera.id for camera in cameras),
                streams_per_camera=conf['streamsPerTestCamera'],
                archive_streams_count=1,
                user=conf['vmsUser'],
                password=conf['vmsPassword'],
                box_ip=box.ip,
                vms_port=vms.port,
            )
            with stream_reader_context_manager as stream_reader_context:
                stream_reader_process = stream_reader_context[0]
                streams = stream_reader_context[1]

                started = False
                stream_opening_started_at = time.time()

                streams_started_flags = dict((stream_id, False) for stream_id, _ in streams.items())

                while time.time() - stream_opening_started_at < 25:
                    if stream_reader_process.poll() is not None:
                        raise exceptions.RtspPerfError("Can't open streams or streaming unexpectedly ended.")

                    line = stream_reader_process.stdout.readline().decode('UTF-8')

                    import re
                    match_res = re.match(r'.*\/([a-z0-9-]+)\?(.*) timestamp (\d+) us$', line.strip())
                    if not match_res:
                        continue

                    rtsp_url_params = dict(
                        [param_pair[0], param_pair[1] if len(param_pair) > 1 else None]
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

                print("All streams opened.")

                streaming_duration_mins = conf['streamingTestDurationMinutes']
                streaming_started_at = time.time()

                last_ptses = {}
                first_tses = {}
                frames = {}
                frame_drops = {}
                lags = {}
                streaming_ended_expected = False
                issues = []
                cpu_usage_max_collector = [None]

                box_poller_thread_stop_event = threading.Event()
                box_poller_thread_exceptions_collector = []

                def box_poller(stop_event, exception_collector, cpu_usage_max_collector):
                    while not stop_event.isSet():
                        try:
                            statistics = api.get_statistics()
                            cannot_get_exception = exceptions.ServerApiError(
                                "Can't get server statistics through the REST API."
                            )
                            if not statistics:
                              raise cannot_get_exception
                            cpu_usage_search_result = [e['value'] for e in statistics if e['description'] == 'CPU']
                            if len(cpu_usage_search_result) != 1:
                                raise cannot_get_exception
                            cpu_usage = cpu_usage_search_result[0]
                            if not cpu_usage_max_collector[0] is None:
                                cpu_usage_max_collector[0] = max(cpu_usage_max_collector[0], cpu_usage)
                            if cpu_usage > ini['cpuUsageThreshold']:
                                raise exceptions.CPUUsageThresholdExceededIssue(
                                    cpu_usage,
                                    ini['cpuUsageThreshold']
                                )
                            stop_event.wait(15)
                        except Exception as e:
                            exception_collector.append(e)
                            return

                box_poller_thread = threading.Thread(
                    target=box_poller,
                    args=(
                        box_poller_thread_stop_event,
                        box_poller_thread_exceptions_collector,
                        cpu_usage_max_collector
                    )
                )

                box_poller_thread.start()

                try:
                    while time.time() - streaming_started_at < streaming_duration_mins*60:
                        if stream_reader_process.poll() is not None:
                            raise exceptions.RtspPerfError("Streaming unxpectedly ended.")

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
                                            'Unexpected error during acquiaring VMS server CPU usage. ' +
                                            'Can be caused by network issues or Server issues.'
                                        ),
                                        original_exception=box_poller_thread_exceptions_collector
                                    )
                                )
                            streaming_ended_expected = True
                            break

                        line = stream_reader_process.stdout.readline().decode('UTF-8')

                        match_res = re.match(r'.*\/([a-z0-9-]+)\?(.*) timestamp (\d+) us$', line.strip())
                        if not match_res:
                            continue

                        rtsp_url_params = dict(
                            [param_pair[0], param_pair[1] if len(param_pair) > 1 else None]
                            for param_pair in [
                                param_pair_str.split('=')
                                for param_pair_str in match_res.group(2).split('&')
                            ]
                        )
                        pts = int(match_res.group(3))

                        pts_stream_id = rtsp_url_params['vms_benchmark_stream_id']

                        frames[pts_stream_id] = frames.get(pts_stream_id, 0) + 1

                        if pts_stream_id not in first_tses:
                            first_tses[pts_stream_id] = time.time()
                        elif streams[pts_stream_id]['type'] == 'live':
                            lags[pts_stream_id] = max(
                                lags.get(pts_stream_id, 0),
                                time.time() - (
                                    first_tses[pts_stream_id] +
                                        frames.get(pts_stream_id, 0) * (1.0/float(ini['testFileFps']))
                                )
                            )

                        ts = time.time()

                        pts_diff_deviation_factor_max = 0.03
                        pts_diff_expected = 1000000./float(ini['testFileFps'])
                        pts_diff = (pts - last_ptses[pts_stream_id]) if pts_stream_id in last_ptses else None
                        pts_diff_max = (1000000./float(ini['testFileFps']))*(1.0 + pts_diff_deviation_factor_max)

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
                                print(f'Detected framedrop from camera {streams[pts_stream_id]["camera_id"]}: ', end='')
                                print(f'{pts_diff} (max={pts_diff_max}) {pts} {time.time()}')

                        if time.time() - streaming_started_at > streaming_duration_mins*60:
                            streaming_ended_expected = True
                            print(f"    Serving {test_cameras_count} virtual cameras succeeded.")
                            break

                        last_ptses[pts_stream_id] = pts
                finally:
                    box_poller_thread_stop_event.set()

                cpu_usage_max = cpu_usage_max_collector[0]

                if not streaming_ended_expected:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        'the stream has unexpectedly finished; ' +
                        'can be caused by network issues or Server issues.',
                        original_exception=issues
                    )

                if time.time() - ts > 5:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        'the stream had been hanged; ' +
                        'can be caused by network issues or Server issues.')

                try:
                    ram_available_bytes = box_platform.ram_available_bytes()
                    ram_free_bytes = ram_available_bytes if ram_available_bytes else box_platform.ram_free_bytes()
                except exceptions.VmsBenchmarkError as e:
                    issues.append(exceptions.UnableToFetchDataFromBox(
                        'Unable to fetch box RAM usage',
                        original_exception=e
                    ))

                frame_drops_sum = sum(frame_drops.values())

                print(f"    Frame drops: {sum(frame_drops.values())} (expected 0)")
                if cpu_usage_max is not None:
                    print(f"    CPU usage: {str(cpu_usage_max) if cpu_usage_max else '-'}")
                if ram_free_bytes is not None:
                    print(f"    Free RAM: {to_megabytes(ram_free_bytes)} MB")

                if frame_drops_sum > 0:
                    issues.append(exceptions.VmsBenchmarkIssue(
                        f'{frame_drops_sum} frame drops detected.'
                    ))

                try:
                    report_server_storage_failures(api, round(streaming_started_at*1000))
                except exceptions.VmsBenchmarkIssue as e:
                    issues.append(e)

                max_allowed_lag_seconds = 5
                max_lag = max(lags.values())
                if max_lag > max_allowed_lag_seconds:
                    issues.append(exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        f'the video lag {round(max_lag)} seconds is more than {max_allowed_lag_seconds} seconds; ' +
                        'can be caused by network issues or poor performance of either the host or the box.'
                    ))

                if len(issues) > 0:
                    raise exceptions.VmsBenchmarkIssue(f'There are {len(issues)} issue(s)', original_exception=issues)

    swapped_after = get_cumulative_swap_bytes(box)
    swapping_threshold_mb = 100
    swapped_during_test = swapped_after - swapped_before
    if swapped_during_test > swapping_threshold_mb * 1024 * 1024:
        raise exceptions.BoxStateError(f"More than {swapping_threshold_mb}M was swapped during the tests")

    print('\nSUCCESS: All tests finished.')
    return 0


def nx_format_exception(exception):
    if isinstance(exception, ValueError):
        return f"Error value {exception}"
    elif isinstance(exception, KeyError):
        return f"No key {exception}"
    elif isinstance(exception, urllib.error.HTTPError):
        if exception.code == 401:
            return 'Server refuses passed credentials: check .conf options vmsUser and vmsPassword'
        else:
            return f'Unexpected HTTP request error (code {exception.code})'
    else:
        return str(exception)


def nx_print_exception(exception, recursive_level=0):
    string_indent = '  '*recursive_level
    if isinstance(exception, exceptions.VmsBenchmarkError):
        print(f"{string_indent}{str(exception)}", file=sys.stderr)
        if exception.original_exception:
            print(f'{string_indent}Caused by:', file=sys.stderr)
            if isinstance(exception.original_exception, list):
                for e in exception.original_exception:
                    nx_print_exception(e, recursive_level=recursive_level+2)
            else:
                nx_print_exception(exception.original_exception, recursive_level=recursive_level+2)
    else:
        print(
            f'{string_indent}{nx_format_exception(exception)}'
            if recursive_level > 0
            else f'{string_indent}ERROR: {nx_format_exception(exception)}'
        )


if __name__ == '__main__':
    print("VMS Benchmark started")
    try:
        logging.info('VMS Benchmark started')
        try:
            sys.exit(main())
        except (exceptions.VmsBenchmarkError, urllib.error.HTTPError) as e:
            print(f'ERROR: ', file=sys.stderr, end='')
            nx_print_exception(e)
            log_exception('ERROR', e)
        except Exception as e:
            print(f'UNEXPECTED ERROR: {e}', file=sys.stderr)
            log_exception('UNEXPECTED ERROR', e)
    except Exception as e:
        print(f'INTERNAL ERROR: {e}', file=sys.stderr)

    sys.exit(1)
