import logging
import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

ini_testcamera_bin: str
ini_test_file_high_resolution: str
ini_test_file_low_resolution: str
ini_testcamera_output_file: str
ini_unloop_via_testcamera: bool
ini_test_file_high_period_us: int
ini_test_file_low_period_us: int
ini_enable_secondary_stream: bool

@contextmanager
def test_camera_running(local_ip, primary_fps, secondary_fps, count=1):
    camera_args = [
        ini_testcamera_bin,
        f"--local-interface={local_ip}",
        '--pts',
        f"--fps-primary", str(primary_fps),
    ]

    if ini_enable_secondary_stream:
        camera_args += [f"--fps-secondary", str(secondary_fps)]
    else:
        camera_args += ["--no-secondary"]

    if ini_enable_secondary_stream:
        secondary_files_string = ';secondary-files="{ini_test_file_low_resolution}"'
    else:
        secondary_files_string = ''

    camera_args += [
        f'files="{ini_test_file_high_resolution}"{secondary_files_string};count={count}'
    ]

    if ini_unloop_via_testcamera:
        camera_args += [
            "--unloop-pts",
            "--shift-pts-primary-period-us", str(ini_test_file_high_period_us),
        ]
        if ini_enable_secondary_stream:
            camera_args += ["--shift-pts-secondary-period-us", str(ini_test_file_low_period_us)]

    opts = {}
    if not ini_testcamera_output_file:
        opts['stdout'] = subprocess.DEVNULL
        opts['stderr'] = subprocess.DEVNULL
    else:
        output_fd = open(ini_testcamera_output_file, 'wb')
        opts['stdout'] = output_fd
        opts['stderr'] = output_fd

    ld_library_path = None
    if platform.system() == 'Linux':
        ld_library_path = os.path.dirname(ini_testcamera_bin)
        opts['env'] = {'LD_LIBRARY_PATH': ld_library_path}

    # NOTE: The first arg is the command itself.
    log_message = 'Running testcamera with the following command and args:\n'
    if ld_library_path:
        log_message += f'    LD_LIBRARY_PATH={ld_library_path}\n'
    for arg in camera_args:
        log_message += f'    {arg}\n'
    logging.info(log_message)

    try:
        proc = subprocess.Popen(camera_args, **opts)
    except Exception as exception:
        raise exceptions.TestCameraError(f"Unable to spawn virtual cameras: {str(exception)}")

    try:
        yield proc
    finally:
        proc.terminate()
