import logging
import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

binary_file = './testcamera/rtsp_perf'
debug = False


@contextmanager
def stream_reader_running(camera_ids, user, password, device_ip, vms_port):
    camera_ids = list(camera_ids)

    args = [
        binary_file,
        '-u',
        user,
        '-p',
        password,
        '--timestamps',
        '--count',
        str(len(camera_ids))
    ]

    for stream_url in (
        f"rtsp://{device_ip}:{vms_port}/{camera_id}"
        for camera_id in camera_ids
    ):
        args.append('--url')
        args.append(stream_url)

    opts = {
        'stdout': subprocess.PIPE,
    }

    if not debug:
        opts['stderr'] = subprocess.PIPE

    ld_library_path = None
    if platform.system() == 'Linux':
        ld_library_path = os.path.dirname(binary_file)
        opts['env'] = {'LD_LIBRARY_PATH': ld_library_path}

    # NOTE: The first arg is the command itself.
    log_message = 'Running rtsp_perf with the following command and args:\n'
    if ld_library_path:
        log_message += f'    LD_LIBRARY_PATH={ld_library_path}\n'
    for arg in args:
        log_message += f'    {arg}\n'
    logging.info(log_message)

    try:
        proc = subprocess.Popen(args, **opts)
    except Exception as exception:
        raise exceptions.RtspPerfError(f"Unexpected error during starting rtsp_perf: {str(exception)}")

    try:
        yield proc
    finally:
        proc.terminate()
