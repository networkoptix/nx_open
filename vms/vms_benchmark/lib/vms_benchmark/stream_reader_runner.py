import logging
import os
import platform
import subprocess
from contextlib import contextmanager

from vms_benchmark import exceptions

binary_file = './testcamera/rtsp_perf'
debug = False


@contextmanager
def stream_reader_running(
    camera_ids,
    streams_per_camera,
    archive_streams_count,
    user,
    password,
    box_ip,
    vms_port
):
    camera_ids = list(camera_ids)

    archive_streams_list = []

    for i in range(archive_streams_count):
        archive_streams_list.append(camera_ids[i % len(camera_ids)])

    args = [
        binary_file,
        '-u',
        user,
        '-p',
        password,
        '--timestamps',
    ]

    streams = {}

    for camera_id, opts in (
        [
            [camera_id, {"type": 'live'}]
            for camera_id in camera_ids*streams_per_camera
        ] + [
            [camera_id, {"type": 'archive'}]
            for camera_id in archive_streams_list
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

        url = base_url + '?' + '&'.join([f"{str(k)}={str(v)}" for k, v in params.items()])

        args.append('--url')
        args.append(url)

        streams[stream_uuid] = {
            "camera_id": camera_id,
            "type": opts.get('type', 'live')
        }

    args.append('--count')
    args.append(len(streams.items()))

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
        proc = subprocess.Popen([str(arg) for arg in args], **opts)
    except Exception as exception:
        raise exceptions.RtspPerfError(f"Unexpected error during starting rtsp_perf: {str(exception)}")

    try:
        yield [proc, streams]
    finally:
        proc.terminate()
