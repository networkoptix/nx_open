#!/usr/bin/env python3

import platform
import signal
import sys
import time
import logging
import traceback

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
import os


def load_configs(config_file):
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
    }

    sys_config = ConfigParser('vms_benchmark.ini', sys_option_descriptions, is_file_optional=True)

    if sys_config['testCameraBin']:
        test_camera_runner.binary_file = sys_config['testCameraBin']
    if sys_config['testFileHighResolution']:
        test_camera_runner.test_file_high_resolution = sys_config['testFileHighResolution']
    if sys_config['testFileLowResolution']:
        test_camera_runner.test_file_low_resolution = sys_config['testFileLowResolution']
    test_camera_runner.debug = sys_config['testCameraDebug']

    return config, sys_config


def log_exception(contextName, exception):
    logging.exception(f'Exception: {contextName}: {type(exception)}: {str(exception)}')


@click.command()
@click.option('--config', 'config_file', default='vms_benchmark.conf', help='Input config file.')
@click.option('-C', 'config_file', default='vms_benchmark.conf', help='Input config file.')
def main(config_file):
    config, sys_config = load_configs(config_file)

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
            f"Can't obtain SSH host key of the box.\nDetails of the error: {res.formatted_message()}"
        )
    else:
        device.host_key = res.details[0]
        device.obtain_connection_info()

    if not device.is_root:
        res = device_tests.SudoConfigured(device).call()

        if not res.success:
            raise exceptions.SshHostKeyObtainingFailed(
                'Sudo is not configured properly, check that user is root or could run `sudo true` ' +
                'without typing password.' +
                f"Details of the error: {res.formatted_message()}"
            )

    print(f"Device IP: {device.ip}")
    if device.is_root:
        print(f"SSH user is root.")

    linux_distribution = LinuxDistributionDetector.detect(device)

    print(f"Linux distribution name: {linux_distribution.name}")
    print(f"Linux distribution version: {linux_distribution.version}")
    print(f"Linux kernel version: {'.'.join(str(c) for c in linux_distribution.kernel_version)}")

    dev_platform = DevicePlatform.gather(device, linux_distribution)

    print(f"Arch: {dev_platform.arch}")
    print(f"Number of CPUs: {dev_platform.cpu_number}")
    print(f"CPU features: {', '.join(dev_platform.cpu_features) if len(dev_platform.cpu_features) > 0 else '-'}")
    print(f"RAM: {dev_platform.ram} ({dev_platform.ram_free()} free)")
    print("Volumes:")
    [
        print(f"    {storage['fs']} on {storage['point']}: free {storage['space_free']} of {storage['space_total']}")
        for (point, storage) in
        dev_platform.storages_list.items()
    ]

    vmses = VmsScanner.scan(device, linux_distribution)

    if vmses and len(vmses) > 0:
        print(f"Detected VMS installations:")
        for vms in vmses:
            print(f"    {vms.customization} in {vms.dir} (port {vms.port}, PID {vms.pid if vms.pid else '-'})")
    else:
        print("There is no VMSes found on the device.")
        print("Nothing to do.")
        return 0

    if len(vmses) > 1:
        raise exceptions.DeviceStateError("More than one customizations found.")

    vms = vmses[0]

    if not vms.is_up():
        raise exceptions.DeviceStateError("VMS is not running currently.")

    print('Restarting server...')
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
    print(f"Free space of RAM: {round(dev_platform.ram_free()/1024/1024)}M of {round(dev_platform.ram/1024/1024)}M")

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

    api.check_authentication()

    if not wait_for_api():
        raise exceptions.ServerApiError("API of Server is not working: ping request was not successful.")

    print('Rest API test is OK.')

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
    memory_per_camera_by_arch = {
        "armv7l": 100,
        "aarch64": 200,
        "x86_64": 200
    }

    for test_cameras_count in config.get('testCamerasTestSequence', [1, 2, 3, 4, 6, 8, 10, 15, 20, 40, 80, 160, 256]):
        ram_available = dev_platform.ram_available()
        ram_free = ram_available if ram_available else dev_platform.ram_free()

        if ram_available and ram_available < (
                test_cameras_count * memory_per_camera_by_arch.get(dev_platform.arch, 200)*1024*1024
        ):
            raise exceptions.InsuficientResourcesError(
                f"{test_cameras_count} is too many test cameras for that box, because the RAM is too small."
            )

        print(f"Try to serve {test_cameras_count} cameras.")
        print("")

        test_camera_cm = test_camera_runner.test_camera_running(
            local_ip=device.local_ip,
            count=test_cameras_count,
            primary_fps=sys_config['testStreamFpsHigh'],
            secondary_fps=sys_config['testStreamFpsLow'],
        )
        with test_camera_cm as test_camera_process:
            print(f"    Spawned {test_cameras_count} test cameras.")

            def wait_test_cameras_discovered(timeout, duration):
                started_at = time.time()
                detection_started_at = None
                while time.time() - started_at < timeout:
                    if test_camera_process.poll() is not None:
                        raise exceptions.TestCameraError(
                            f'Test Camera process exited unexpectedly with code {test_camera_process.returncode}')

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

                print(f"    Waiting for test cameras discovering... (timeout is {discovering_timeout} seconds)")
                res, cameras = wait_test_cameras_discovered(discovering_timeout, 3)
                if not res:
                    raise exceptions.TestCameraError('Timeout expired.')

                print("    All test cameras had been discovered successfully.")

                time.sleep(test_cameras_count * 2)

                for camera in cameras:
                    if camera.enable_recording(highStreamFps=sys_config['testStreamFpsHigh']):
                        print(f"    Recording on camera {camera.id} enabled.")
                    else:
                        raise exceptions.TestCameraError(f"Error enabling recording on camera {camera.id}: request failed.")

                camera_id = cameras[0].id
            except Exception as e:
                log_exception('Discovering cameras', e)
                raise exceptions.TestCameraError(f"Cameras are not discovered.", e) from e

            try:
                stream_opening_started_at = time.time()
                stream_duration = sys_config['streamTestDurationSecs']

                while time.time() - stream_opening_started_at < 25:
                    stream_url = f"rtsp://{config['vmsUser']}:{config['vmsPassword']}@{device.ip}:{vms.port}/{camera_id}"
                    stream = cv2.VideoCapture(stream_url)

                    if stream.isOpened():
                        print(f"    RTSP stream opened. Test duration: {stream_duration} seconds.")
                        break

                if not stream.isOpened():
                    raise exceptions.TestCameraStreamingError(f"Can't open RTSP stream {stream_url}: timeout ended")

                streaming_started_at = time.time()

                last_pts = None
                last_ts = None
                first_ts = None
                frames = 0

                frame_drops = 0
                dummy_frames_count = 0

                while stream.isOpened():
                    _ret, _frame = stream.read()
                    frames += 1
                    pts = stream.get(cv2.CAP_PROP_POS_MSEC)

                    if first_ts is None:
                        first_ts = time.time()
                    else:
                        lag = time.time() - (first_ts + frames * (1.0/float(sys_config['testFileFps'])))
                        if lag > 5:
                            raise exceptions.TestCameraStreamingError('Stream is too slow.')

                    ts = time.time()

                    if last_ts is not None and ts - last_ts < 0.01:
                        dummy_frames_count += 1

                        if dummy_frames_count > 50:
                            raise exceptions.TestCameraStreamingError('Stream is unexpectedly broken.')

                    pts_diff_max = (1000./float(sys_config['testFileFps']))*1.05

                    if last_pts is not None and pts - last_pts > pts_diff_max:
                        frame_drops += 1
                    if time.time() - streaming_started_at > stream_duration:
                        print(f"    Serving {test_cameras_count} cameras is OK.")
                        break
                    last_pts = pts
                    last_ts = time.time()
                import pprint
                log_events = api.get_events(streaming_started_at)
                storage_failure_events_count = sum(
                    event['aggregationCount']
                    for event in log_events
                    if event['eventParams'].get('eventType', None) == 'storageFailureEvent'
                )
                print(f"    Frame drops: {frame_drops}")
                print(f"    Storage failures: {storage_failure_events_count}")
                print(f"    CPU usage: {device.eval('cat /proc/loadavg').split()[1]}")
                print(f"    Free RAM: {round(ram_free/1024/1024)}M")
            except exceptions.TestCameraStreamingError as exception:
                print(f"\nISSUE: {str(exception)}")
                log_exception("ISSUE", exception)
                return 1
            finally:
                stream.release()

    print('\nAll tests are successfully finished.')
    return 0


def nx_print_exception(exception):
    if isinstance(exception, exceptions.VmsBenchmarkError):
        print(f"{str(exception)}", file=sys.stderr)
        if exception.original_exception:
            print('Caused by:', file=sys.stderr)
            nx_print_exception(exception.original_exception)
    elif isinstance(exception, urllib.error.HTTPError):
        if exception.code == 401:
            print(f'Server refuses passed credentials: check .conf options vmsUser and vmsPassword.', file=sys.stderr)
        else:
            print(f'Unexpected HTTP request error (code {exception.code}.', file=sys.stderr)
    else:
        print(f'ERROR: {exception}')


if __name__ == '__main__':
    print("VMS Benchmark started")
    try:
        logging.info('VMS Benchmark started')
        try:
            sys.exit(main())
        except urllib.error.HTTPError as e:
            print(f'ERROR: Server reported HTTP code {e.code}')
        except exceptions.VmsBenchmarkError as e:
            print(f'ERROR: ', file=sys.stderr, end='')
            nx_print_exception(e)
            log_exception('ERROR', e)
    except Exception as e:
        print(f'INTERNAL ERROR: {e}', file=sys.stderr)
        log_exception('INTERNAL ERROR', e)
    finally:
        sys.exit(1)
