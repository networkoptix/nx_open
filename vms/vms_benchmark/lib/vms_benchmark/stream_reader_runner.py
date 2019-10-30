import logging
import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

ini_rtsp_perf_bin: str
ini_rtsp_perf_stderr_file: str


@contextmanager
def stream_reader_running(
    camera_ids,
    total_live_stream_count: int,
    total_archive_stream_count: int,
    user,
    password,
    box_ip,
    vms_port,
    archive_read_pos_ms_utc: int,
):
    args = [
        ini_rtsp_perf_bin,
        '-u',
        user,
        '-p',
        password,
        '--timestamps',
        '--disable-restart',
    ]

    streams = {}

    for type, count in ('live', total_live_stream_count), ('archive', total_archive_stream_count):
        for i in range(count):
            camera_id = camera_ids[i % len(camera_ids)]
            stream_id = f'{type}{i:03d}'

            base_url = f"rtsp://{box_ip}:{vms_port}/{camera_id}"

            params = {
                'vms_benchmark_stream_id': stream_id,
            }

            if type == 'archive':
                params['pos'] = archive_read_pos_ms_utc

            params['stream'] = 0  # Request primary stream.

            url = base_url + '?' + '&'.join([f"{str(k)}={str(v)}" for k, v in params.items()])

            args.append('--url')
            args.append(url)

            streams[stream_id] = {
                "camera_id": camera_id,
                "type": type
            }

    # E.g. if 3 URLs are passed to rtsp_perf and --count is 5, it may open streams 1, 1, 2, 2, 3.
    args.append('--count')
    args.append(len(streams.items()))

    opts = {'stdout': subprocess.PIPE}
    if not ini_rtsp_perf_stderr_file:
        opts['stderr'] = subprocess.DEVNULL
    else:
        output_fd = open(ini_rtsp_perf_stderr_file, 'wb')
        opts['stderr'] = output_fd

    ld_library_path = None
    if platform.system() == 'Linux':
        ld_library_path = os.path.dirname(ini_rtsp_perf_bin)
        opts['env'] = {'LD_LIBRARY_PATH': ld_library_path}

    # NOTE: The first arg is the command itself.
    log_message = 'Running rtsp_perf with the following command and args:\n'
    if ld_library_path:
        log_message += f'    LD_LIBRARY_PATH={ld_library_path}\n'
    for arg in args:
        log_message += f'    {arg}\n'
    logging.info(log_message)

    try:
        proc = subprocess.Popen([str(arg) for arg in args], **opts)
    except Exception as exception:
        raise exceptions.RtspPerfError(f"Unable to start rtsp_perf: {str(exception)}")

    try:
        yield [proc, streams]
    finally:
        proc.terminate()
