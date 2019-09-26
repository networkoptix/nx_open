#!/usr/bin/env python3

import platform
import signal
import sys
import time
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
from vms_benchmark.device_connection import DeviceConnection
from vms_benchmark.device_platform import DevicePlatform
from vms_benchmark.vms_scanner import VmsScanner
from vms_benchmark.server_api import ServerApi
from vms_benchmark import test_camera_runner
from vms_benchmark import stream_reader_runner
from vms_benchmark.linux_distibution import LinuxDistributionDetector
from vms_benchmark import device_tests
from vms_benchmark import host_tests
from vms_benchmark import exceptions

import urllib
import click


def to_megabytes(bytes_count):
    return bytes_count // (1024 * 1024)


def to_percentage(share_0_1):
    return round(share_0_1 * 100)


def load_configs(config_file, sys_config_file):
    option_descriptions = {
        "deviceHost": {
            "type": 'string'
        },
        "deviceLogin": {
            "optional": True,
            "type": 'string'
        },
        "devicePassword": {
            "optional": True,
            "type": 'string'
        },
        "deviceSshPort": {
            "optional": True,
            "type": 'integer',
            "range": [0, 31768],
            "default": 22
        },
        "vmsUser": {
            "type": "string"
        },
        "vmsPassword": {
            "type": "string"
        },
        "testCamerasTestSequence": {
            "optional": True,
            "type": "integers"
        },
        "streamsPerTestCamera": {
            "optional": True,
            "type": 'integer'
        },
    }

    config = ConfigParser(config_file, option_descriptions)

    sys_option_descriptions = {
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
        "streamTestDurationSecs": {
            "optional": True,
            "type": 'integer',
            "default": 60
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

    sys_config = ConfigParser(sys_config_file, sys_option_descriptions, is_file_optional=True)

    if sys_config['testCameraBin']:
        test_camera_runner.binary_file = sys_config['testCameraBin']
    if sys_config['rtspPerfBin']:
        stream_reader_runner.binary_file = sys_config['rtspPerfBin']
    if sys_config['testFileHighResolution']:
        test_camera_runner.test_file_high_resolution = sys_config['testFileHighResolution']
    if sys_config['testFileLowResolution']:
        test_camera_runner.test_file_low_resolution = sys_config['testFileLowResolution']
    test_camera_runner.debug = sys_config['testCameraDebug']

    return config, sys_config


def log_exception(context_name, exception):
    logging.exception(f'Exception: {context_name}: {type(exception)}: {str(exception)}')


def device_combined_cpu_usage(device):
    cpu_usage_file = '/proc/loadavg'
    cpu_usage_reply = device.eval(f'cat {cpu_usage_file}')
    cpu_usage_reply_list = cpu_usage_reply.split() if cpu_usage_reply else None

    if not cpu_usage_reply_list or len(cpu_usage_reply_list) < 1:
        # TODO: Properly log an arbitrary string in cpu_usage_reply.
        raise exceptions.DeviceFileContentError(cpu_usage_file)

    try:
        # Get CPU usage average during the last minute.
        result = float(cpu_usage_reply_list[0])
    except ValueError:
        raise exceptions.DeviceFileContentError(cpu_usage_file)

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


def get_writable_storages(api, sys_config):
    try:
        storages = api.get_storage_spaces()
    except exceptions.VmsBenchmarkError as e:
        raise exceptions.ServerApiError('Unable to get VMS storages via REST HTTP', original_exception=e)

    if storages is None:
        raise exceptions.ServerApiError('Unable to get VMS storages via REST HTTP: request failed')

    def storage_is_writable(storage):
        free_space = int(storage['freeSpace'])
        reserved_space = int(storage['reservedSpace'])

        return free_space >= reserved_space + sys_config['minimumStorageFreeBytes']

    try:
        result = [storage for storage in storages if storage_is_writable(storage)]
    except (ValueError, KeyError) as e:
        raise exceptions.ServerApiResponseError("Bad response body for storage info request", original_exception=e)

    return result


@click.command()
@click.option('--config', 'config_file', default='vms_benchmark.conf', help='Input config file.')
@click.option('-C', 'config_file', default='vms_benchmark.conf', help='Input config file.')
@click.option('--sys-config', 'sys_config_file', default='vms_benchmark.ini', help='Input internal system config file.')
def main(config_file, sys_config_file):
    # TODO: Rename dictionaries to `conf` and `ini` respectively, and files to `conf_file` and `ini_file`.
    config, sys_config = load_configs(config_file, sys_config_file)

    if sys_config.ORIGINAL_OPTIONS is not None:
        print(f"Override default options from INI config '{sys_config_file}':")
        for k, v in sys_config.ORIGINAL_OPTIONS.items():
            print(f"    {k}={v}")

    password = config.get('devicePassword', None)

    if password and platform.system() == 'Linux':
        res = host_tests.SshPassInstalled().call()

        if not res.success:
            raise exceptions.HostPrerequisiteFailed(
                "ERROR: sshpass is not on PATH" +
                " (check if it is installed; to install on Ubuntu: `sudo apt install sshpass`)." +
                f"Details for the error: {res.formatted_message()}"
            )

    device = DeviceConnection(
        host=config['deviceHost'],
        login=config.get('deviceLogin', None),
        password=password,
        port=config['deviceSshPort']
    )

    res = device_tests.SshHostKeyIsKnown(device).call()

    if not res.success:
        raise exceptions.SshHostKeyObtainingFailed(
            f"Can't obtain ssh host key of the box." +
            (f"\n    Details: {res.formatted_message()}" if res.formatted_message() else "")
        )
    else:
        device.host_key = res.details[0]
        device.obtain_connection_info()

    if not device.is_root:
        res = device_tests.SudoConfigured(device).call()

        if not res.success:
            raise exceptions.SshHostKeyObtainingFailed(
                'Sudo is not configured properly, check that user is root or can run `sudo true` ' +
                'without typing a password.\n' +
                f"Details of the error: {res.formatted_message()}"
            )

    print(f"Device IP: {device.ip}")
    if device.is_root:
        print(f"ssh user is root.")

    linux_distribution = LinuxDistributionDetector.detect(device)

    print(f"Linux distribution name: {linux_distribution.name}")
    print(f"Linux distribution version: {linux_distribution.version}")
    print(f"Linux kernel version: {'.'.join(str(c) for c in linux_distribution.kernel_version)}")

    dev_platform = DevicePlatform.gather(device, linux_distribution)

    print(f"Arch: {dev_platform.arch}")
    print(f"Number of CPUs: {dev_platform.cpu_count}")
    print(f"CPU features: {', '.join(dev_platform.cpu_features) if len(dev_platform.cpu_features) > 0 else '-'}")
    print(f"RAM: {to_megabytes(dev_platform.ram_bytes)} MB ({to_megabytes(dev_platform.ram_free_bytes())} MB free)")
    print("Volumes:")
    [
        print(f"    {storage['fs']} on {storage['point']}: free {storage['space_free']} of {storage['space_total']}")
        for (point, storage) in
        dev_platform.storages_list.items()
    ]

    vmses = VmsScanner.scan(device, linux_distribution)

    if vmses and len(vmses) > 0:
        print(f"Detected VMS installation(s):")
        for vms in vmses:
            print(f"    {vms.customization} in {vms.dir} (port {vms.port}, PID {vms.pid if vms.pid else '-'})")
    else:
        print("No VMS installations found on the box.")
        print("Nothing to do.")
        return 0

    if len(vmses) > 1:
        raise exceptions.DeviceStateError("More than one customizations found.")

    vms = vmses[0]

    if not vms.is_up():
        raise exceptions.DeviceStateError("VMS is not running currently.")

    print('Restarting Server...')
    vms.restart(exc=True)

    def wait_for_server_up(timeout=30):
        started_at = time.time()

        while True:
            global vmses

            vmses = VmsScanner.scan(device, linux_distribution)

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
    ram_free_bytes = dev_platform.ram_available_bytes()
    if ram_free_bytes is None:
        ram_free_bytes = dev_platform.ram_free_bytes()
    print('Server restarted successfully.')
    print(f"RAM free: {to_megabytes(ram_free_bytes)} MB of {to_megabytes(dev_platform.ram_bytes)} MB")

    api = ServerApi(device.ip, vms.port, user=config['vmsUser'], password=config['vmsPassword'])

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
        storages = get_writable_storages(api, sys_config)
    except Exception as e:
        raise exceptions.ServerApiError(message="Unable to get VMS storages via REST HTTP", original_exception=e)

    if len(storages) == 0:
        raise exceptions.DeviceStateError("Server has no suitable video archive storage, check the free space")

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

    for test_cameras_count in config.get(
            'testCamerasTestSequence', [1, 2, 3, 4, 6, 8, 10, 16, 32, 64, 128]):

        ram_available_bytes = dev_platform.ram_available_bytes()
        ram_free_bytes = ram_available_bytes if ram_available_bytes else dev_platform.ram_free_bytes()

        if ram_available_bytes and ram_available_bytes < (
                test_cameras_count * ram_bytes_per_camera_by_arch.get(dev_platform.arch, 200) * 1024 * 1024
        ):
            raise exceptions.InsuficientResourcesError(
                f"Not enough free RAM on the box for {test_cameras_count} cameras.")

        print(f"Serving {test_cameras_count} virtual cameras.")
        print("")

        test_camera_context_manager = test_camera_runner.test_camera_running(
            local_ip=device.local_ip,
            count=test_cameras_count,
            primary_fps=sys_config['testStreamFpsHigh'],
            secondary_fps=sys_config['testStreamFpsLow'],
        )
        with test_camera_context_manager as test_camera_context:
            print(f"    Started {test_cameras_count} virtual cameras.")

            def wait_test_cameras_discovered(timeout, duration):
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
                        elif time.time() - detection_started_at >= duration:
                            return True, cameras
                    else:
                        detection_started_at = None

                    time.sleep(1)
                return False, None

            try:
                discovering_timeout = 120

                print(f"    Waiting for virtual cameras discovery and going live (timeout is {discovering_timeout} seconds).")
                res, cameras = wait_test_cameras_discovered(discovering_timeout, 3)
                if not res:
                    raise exceptions.TestCameraError('Timeout expired.')

                print("    All virtual cameras discovered and went live.")

                time.sleep(test_cameras_count * 2)

                for camera in cameras:
                    if camera.enable_recording(highStreamFps=sys_config['testStreamFpsHigh']):
                        print(f"    Recording on camera {camera.id} enabled.")
                    else:
                        raise exceptions.TestCameraError(
                            f"Failed enabling recording on camera {camera.id}.")
            except Exception as e:
                log_exception('Discovering cameras', e)
                raise exceptions.TestCameraError(f"Not all virtual cameras were discovered or went live.", e) from e

            stream_reader_context_manager = stream_reader_runner.stream_reader_running(
                camera_ids=(camera.id for camera in cameras),
                streams_per_camera=config.get('streamsPerTestCamera', len(cameras)),
                user=config['vmsUser'],
                password=config['vmsPassword'],
                device_ip=device.ip,
                vms_port=vms.port
            )
            with stream_reader_context_manager as stream_reader_context:
                started = False
                stream_opening_started_at = time.time()

                cameras_started_flags = dict((camera.id, False) for camera in cameras)

                while time.time() - stream_opening_started_at < 25:
                    if stream_reader_context.poll() is not None:
                        raise exceptions.RtspPerfError('FOOO')

                    line = stream_reader_context.stdout.readline().decode('UTF-8')
                    import re
                    match = re.match(r'.* ([a-z0-9-]+) timestamp (\d+) us$', line.strip())
                    if not match:
                        continue

                    camera_id = match.group(1)

                    if camera_id not in cameras_started_flags:
                        raise exceptions.RtspPerfError(f'Cannot open video streams: unexpected camera id.')

                    cameras_started_flags[camera_id] = True

                    if len(list(filter(lambda e: e[1] is False, cameras_started_flags.items()))) == 0:
                        started = True
                        break

                if started is False:
                    # TODO: check all streams are opened
                    raise exceptions.TestCameraStreamingIssue(
                        f'Cannot open video streams: timeout expired.')

                print("All streams opened.")

                stream_duration = sys_config['streamTestDurationSecs']
                streaming_started_at = time.time()

                last_ptses = {}
                first_tses = {}
                frames = {}
                frame_drops = {}
                lags = {}
                streaming_succeeded = False

                while time.time() - streaming_started_at < stream_duration:
                    if stream_reader_context.poll() is not None:
                        raise exceptions.RtspPerfError('asdf')

                    line = stream_reader_context.stdout.readline().decode('UTF-8')

                    match_res = re.match(r'.* ([a-z0-9-]+) timestamp (\d+) us$', line.strip())
                    if not match_res:
                        continue

                    pts = int(match_res.group(2))/1000

                    pts_camera_id = match_res.group(1)

                    frames[pts_camera_id] = frames.get(pts_camera_id, 0) + 1

                    if pts_camera_id not in first_tses:
                        first_tses[pts_camera_id] = time.time()
                    else:
                        lags[pts_camera_id] = max(
                            lags.get(pts_camera_id, 0),
                            time.time() - (first_tses[pts_camera_id] + frames.get(pts_camera_id, 0) * (1.0/float(sys_config['testFileFps'])))
                        )

                    ts = time.time()

                    pts_diff_deviation_factor_max = 0.01
                    pts_diff_expected = 1000.0/float(sys_config['testFileFps'])
                    pts_diff = pts - last_ptses[pts_camera_id] if pts_camera_id in last_ptses else None
                    pts_diff_max = (1000./float(sys_config['testFileFps']))*1.05

                    # The value is negative because the first PTS of new loop is less than last PTS of the previous
                    # loop.
                    pts_diff_expected_new_loop = -float(sys_config['testFileHighDurationMs'])

                    if pts_diff is not None:
                        pts_diff_deviation_factor = lambda diff_expected: abs((diff_expected - pts_diff) / diff_expected)

                        if (
                                pts_diff_deviation_factor(pts_diff_expected) > pts_diff_deviation_factor_max and
                                pts_diff_deviation_factor(pts_diff_expected_new_loop) > pts_diff_deviation_factor_max
                        ):
                            frame_drops[pts_camera_id] = frame_drops.get(pts_camera_id, 0) + 1
                            print(f'Detected framedrop from camera {pts_camera_id}: {pts - last_ptses.get(pts_camera_id, pts)} (max={pts_diff_max}) {pts} {time.time()}')

                    if time.time() - streaming_started_at > stream_duration:
                        streaming_succeeded = True
                        print(f"    Serving {test_cameras_count} virtual cameras succeeded.")
                        break

                    last_ptses[pts_camera_id] = pts

                if not streaming_succeeded:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        'the stream has unexpectedly finished; ' +
                        'can be caused by network issues or Server issues.')

                if time.time() - ts > 5:
                    raise exceptions.TestCameraStreamingIssue(
                        'Streaming video from the Server FAILED: ' +
                        'the stream had been hanged; ' +
                        'can be caused by network issues or Server issues.')

                issues = []

                try:
                    ram_available_bytes = dev_platform.ram_available_bytes()
                    ram_free_bytes = ram_available_bytes if ram_available_bytes else dev_platform.ram_free_bytes()
                except exceptions.VmsBenchmarkError as e:
                    issues.append(exceptions.UnableToFetchDataFromDevice(
                        'Unable to fetch box RAM usage',
                        original_exception=e
                    ))

                try:
                    cpu_usage_summarized = device_combined_cpu_usage(device) / dev_platform.cpu_count
                except exceptions.VmsBenchmarkError as e:
                    issues.append(exceptions.UnableToFetchDataFromDevice(
                        'Unable to fetch box cpu usage',
                        original_exception=e
                    ))
                    cpu_usage_summarized = None

                frame_drops_sum = sum(frame_drops.values())

                print(f"    Frame drops: {sum(frame_drops.values())} (expected 0)")
                if cpu_usage_summarized is not None:
                    print(f"    CPU usage: {str(cpu_usage_summarized) if cpu_usage_summarized else '-'}")
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

                if cpu_usage_summarized is not None and cpu_usage_summarized > sys_config['cpuUsageThreshold']:
                    issues.append(exceptions.CPUUsageThresholdExceededIssue(
                        cpu_usage_summarized,
                        sys_config['cpuUsageThreshold']
                    ))

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
