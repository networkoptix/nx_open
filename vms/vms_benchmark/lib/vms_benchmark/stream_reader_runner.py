import logging
import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

ini_rtsp_perf_bin: str
ini_rtsp_perf_print_stderr: bool


@contextmanager
def stream_reader_running(
    camera_ids,
    total_live_stream_count: int,
    total_archive_stream_count: int,
    user,
    password,
    box_ip,
    vms_port
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

    def make_number_of_ids(count):
        return (camera_ids[i % len(camera_ids)] for i in range(count))

    for camera_id, opts in (
        [
            [camera_id, {"type": 'live'}]
            for camera_id in make_number_of_ids(total_live_stream_count)
        ] + [
            [camera_id, {"type": 'archive'}]
            for camera_id in make_number_of_ids(total_archive_stream_count)
        ]
    ):
        import uuid
        stream_uuid = str(uuid.uuid4())

        base_url = f"rtsp://{box_ip}:{vms_port}/{camera_id}"

        params = {
            'vms_benchmark_stream_id': stream_uuid,
        }

        if opts.get('type', 'live') == 'archive':
            params['pos'] = 0

        params['stream'] = 0  # Request primary stream.

        url = base_url + '?' + '&'.join([f"{str(k)}={str(v)}" for k, v in params.items()])

        args.append('--url')
        args.append(url)

        streams[stream_uuid] = {
            "camera_id": camera_id,
            "type": opts.get('type', 'live')
        }

    # E.g. if 3 URLs are passed to rtsp_perf and --count is 5, it may open streams 1, 1, 2, 2, 3.
    args.append('--count')
    args.append(len(streams.items()))

    opts = {
        'stdout': subprocess.PIPE,
    }

    if not ini_rtsp_perf_print_stderr:
        opts['stderr'] = subprocess.PIPE

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
