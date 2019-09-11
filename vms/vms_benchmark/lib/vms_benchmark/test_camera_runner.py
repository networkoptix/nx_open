import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

binary_file = './testcamera'
test_file_high_resolution = './test_file_high_resolution.ts'
test_file_low_resolution = './test_file_low_resolution.ts'
debug = False


@contextmanager
def test_camera_running(local_ip, lowStreamFps, count=1):
    camera_args = [
        binary_file,
        f"--local-interface={local_ip}",
        f"--fps {lowStreamFps}",
        f"files=\"{test_file_high_resolution}\";secondary-files=\"{test_file_low_resolution}\";count={count}",
    ]

    opts = {}

    if not debug:
        opts['stdout'] = subprocess.PIPE
        opts['stderr'] = subprocess.PIPE

    env = {}
    if platform.system() == 'Linux':
        env['LD_LIBRARY_PATH'] = os.path.dirname(binary_file)

    try:
        proc = subprocess.Popen(camera_args, env=env, **opts)
    except Exception as exception:
        raise exceptions.TestCameraError(f"Unexpected error during spawning cameras: {str(exception)}")

    try:
        yield proc
    finally:
        proc.terminate()
