#!/usr/bin/env python3

import platform
import signal
import sys
import time
import logging
from dataclasses import dataclass

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
from vms_benchmark.linux_distibution import LinuxDistributionDetector
from vms_benchmark import device_tests
from vms_benchmark import host_tests
from vms_benchmark import exceptions

import urllib
import click
import cv2


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
        }
    }

    config = ConfigParser(config_file, option_descriptions)

    sys_option_descriptions = {
        "testCameraBin": {
            "optional": True,
            "type": 'string',
            "default": './testcamera/testcamera'
        },
        "testFileHighResolution": {
            "optional": True,
            "type": 'string',
            "default": './high.ts'
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

    return storage_failure_events_count


@dataclass
class Stats:
    dropped_frames_count: int = 0
    dup_frames_count: int = 0
    cpu_usage_per_cpu: int = 0
    ram_free_bytes: int = 0


def report_stats(stats, api, streaming_started_at, sys_config):
    print(f"    Frame drops: {stats.dropped_frames_count} (expected 0)")
    print(f"    Frames with equal timestamps: {stats.dup_frames_count} (expected 0)")
    print(f"    CPU usage (per-CPU average): {to_percentage(stats.cpu_usage_per_cpu)}%")
    print(f"    RAM free before discovering cameras: {to_megabytes(stats.ram_free_bytes)} MB")

    storage_failure_count = report_server_storage_failures(api, streaming_started_at)

    # Collect messages for all detected issues and join them via '; '.

    issue_message = ""

    if storage_failure_count > 0:
        issue_message += (('; ' if issue_message else '') +
             f"{storage_failure_count} Server Storage failure events")

    if stats.dropped_frames_count > 0:
        issue_message += (('; ' if issue_message else '') +
            f"{stats.dropped_frames_count} dropped frame(s)")

    if stats.dup_frames_count > 0:
        issue_message += (('; ' if issue_message else '') +
            f"{stats.dup_frames_count} frame(s) with equal timestamps")

    if stats.cpu_usage_per_cpu > sys_config['cpuUsageThreshold']:
        issue_message += (('; ' if issue_message else '') +
            f"box CPU usage of {to_percentage(stats.cpu_usage_per_cpu)}% is too high " +
            f"(exceeds {to_percentage(sys_config['cpuUsageThreshold'])}%)")

    if issue_message:
        raise exceptions.VmsBenchmarkIssue("Detected the following: " + issue_message + ".")


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
            f"Can't obtain ssh host key of the box.\nDetails of the error: {res.formatted_message()}"
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
    print('Server restarted successfully.')
    print(f"RAM free: {to_megabytes(dev_platform.ram_free_bytes())} MB of {to_megabytes(dev_platform.ram_bytes)} MB")

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

        stats = Stats()

        ram_available_bytes = dev_platform.ram_available_bytes()
        stats.ram_free_bytes = ram_available_bytes if ram_available_bytes else dev_platform.ram_free_bytes()

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

                camera_id = cameras[0].id
            except Exception as e:
                log_exception('Discovering cameras', e)
                raise exceptions.TestCameraError(f"Not all virtual cameras were discovered or went live.", e) from e

            try:
                stream_opening_started_at = time.time()
                stream_duration = sys_config['streamTestDurationSecs']

                while time.time() - stream_opening_started_at < 25:
                    stream_url = f"rtsp://{config['vmsUser']}:{config['vmsPassword']}@{device.ip}:{vms.port}/{camera_id}"
                    stream = cv2.VideoCapture(stream_url)

                    if stream.isOpened():
                        print(f"    Video stream from the Server opened. Test duration: {stream_duration} seconds.")
                        break

                if not stream.isOpened():
                    raise exceptions.TestCameraStreamingIssue(
                        f'Cannot open video stream {stream_url}: timeout expired.')

                streaming_started_at = time.time()

                last_pts = None
                last_ts = None
                first_ts = None
                frames = 0

                try:
                    streaming_succeeded = False
                    while stream.isOpened():
                        _ret, _frame = stream.read()
                        frames += 1
                        pts = stream.get(cv2.CAP_PROP_POS_MSEC)

                        if first_ts is None:
                            first_ts = time.time()
                        else:
                            lag = time.time() - (first_ts + frames * (1.0/float(sys_config['testFileFps'])))
                            max_allowed_lag_seconds = 5
                            if lag > max_allowed_lag_seconds:
                                raise exceptions.TestCameraStreamingIssue(
                                    'Streaming video from the Server FAILED: ' +
                                    f'the video lags for more than {max_allowed_lag_seconds} seconds; ' +
                                    'can be caused by network issues or poor performance of either the host or the box.'
                                )

                        ts = time.time()

                        if last_ts is not None and ts - last_ts < 0.01:
                            stats.dup_frames_count += 1

                            max_allowed_dummy_frames = 50
                            if stats.dup_frames_count > max_allowed_dummy_frames:
                                raise exceptions.TestCameraStreamingIssue(
                                    'Streaming video from the Server FAILED: ' +
                                    f'the stream is broken - more than {max_allowed_dummy_frames} ' +
                                    'frames with equal timestamps; ' +
                                    'can be caused by poor performance of either the host or the box, ' +
                                    'or Server issues.')

                        pts_diff_max = (1000./float(sys_config['testFileFps']))*1.05

                        if last_pts is not None and pts - last_pts > pts_diff_max:
                            stats.dropped_frames_count += 1
                        if time.time() - streaming_started_at > stream_duration:
                            streaming_succeeded = True
                            print(f"    Serving {test_cameras_count} virtual cameras succeeded.")
                            break
                        last_pts = pts
                        last_ts = time.time()

                    if not streaming_succeeded:
                        raise exceptions.TestCameraStreamingIssue(
                            'Streaming video from the Server FAILED: ' +
                            'the stream has unexpectedly finished; ' +
                            'can be caused by network issues or Server issues.')
                finally:
                    try:
                        stats.cpu_usage_per_cpu = device_combined_cpu_usage(device) / dev_platform.cpu_count
                    except exceptions.VmsBenchmarkError as e:
                        raise exceptions.UnableToFetchDataFromDevice(
                            'Unable to fetch box cpu usage',
                            original_exception=e
                        )

                    report_stats(stats, api, streaming_started_at, sys_config)
            except exceptions.VmsBenchmarkIssue as exception:
                print(f"\nISSUE: {str(exception)}")
                log_exception("ISSUE", exception)
                return 1
            finally:
                stream.release()

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
