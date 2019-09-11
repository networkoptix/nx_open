import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

binary_file = './testcamera'
test_file_high_resolution = './test_file_high_resolution.ts'
test_file_low_resolution = './test_file_low_resolution.ts'
debug = False
logging = None


@contextmanager
def test_camera_running(local_ip, primary_fps, secondary_fps, count=1):
    camera_args = [
        binary_file,
        f"--local-interface={local_ip}",
        f"--fps-primary {primary_fps}",
        f"--fps-secondary {secondary_fps}",
        f"files=\"{test_file_high_resolution}\";secondary-files=\"{test_file_low_resolution}\";count={count}",
    ]

    opts = {}

    if not debug:
        opts['stdout'] = subprocess.PIPE
        opts['stderr'] = subprocess.PIPE

    env = {}
    ld_library_path = None
    if platform.system() == 'Linux':
        ld_library_path = os.path.dirname(binary_file)
        env['LD_LIBRARY_PATH'] = ld_library_path

    assert logging
    # NOTE: The first arg is the command itself.
    logging.info('Running testcamera with the following command and args:')
    logging.info('[')
    if ld_library_path:
        logging.info(f'LD_LIBRARY_PATH={ld_library_path}')
    for arg in camera_args:
        logging.info(arg)
    logging.info(']')

    try:
        proc = subprocess.Popen(camera_args, env=env, **opts)
    except Exception as exception:
        raise exceptions.TestCameraError(f"Unexpected error during spawning cameras: {str(exception)}")

    try:
        yield proc
    finally:
        proc.terminate()
