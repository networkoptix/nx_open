"""Camera support classes

Server has separate protocol created specifically for test cameras. It multicasts UDP packets to port 4984
and expects UDP responses from test cameras, with camera mac address and TCP endpoint for media streaming.
Then it connects to that endpoint using TCP, with one-line request and expects media stream with specific
formatting in response.
All this is supported by 3 classes:
* DiscoveryUdpListener - listens and responds to UDP requets
* MediaListener - listens on TCP port to receive media stream requests
* MediaStreamer - reads TCP request on connected socket and sends media stream from file.
All these 3 classes are internal for this module; Tests only see and use Camera and CameraFactory instances,
created using 'camera' or 'camera_pool' fixtures.
"""
from __future__ import division

import errno
import logging
import select
import socket
import timeit
from collections import deque

import hachoir_core.config
import ifaddr
from contextlib2 import ExitStack
from typing import List, Tuple

# overwise hachoir will replace sys.stdout/err with UnicodeStdout, incompatible with pytest terminal module:
hachoir_core.config.unicode_stdout = False
import hachoir_parser
import hachoir_metadata

_logger = logging.getLogger(__name__)

TEST_CAMERA_FIND_MSG = 'Network optix camera emulator 3.0 discovery'  # UDP discovery multicast request
TEST_CAMERA_ID_MSG = 'Network optix camera emulator 3.0 responce'  # UDP discovery response from test camera


def make_camera_info(parent_id, name, mac_addr):
    return dict(
        audioEnabled=False,
        controlEnabled=False,
        dewarpingParams='',
        groupId='',
        groupName='',
        mac=mac_addr,
        manuallyAdded=False,
        maxArchiveDays=0,
        minArchiveDays=0,
        # model=name,
        model='TestCameraLive',
        motionMask='',
        motionType='MT_Default',
        name=name,
        parentId=parent_id,
        physicalId=mac_addr,
        preferredServerId='{00000000-0000-0000-0000-000000000000}',
        scheduleEnabled=False,
        scheduleTasks=[],
        secondaryStreamQuality='SSQualityLow',
        status='Unauthorized',
        statusFlags='CSF_NoFlags',
        # typeId='{7d2af20d-04f2-149f-ef37-ad585281e3b7}',
        typeId='{f9c03047-72f1-4c04-a929-8538343b6642}',
        url='127.0.0.100',
        vendor='python-funtest',
        )


def _close_all(resources):
    # Use ExitStack because of its precise exception handling.
    exit_stack = ExitStack()
    for resource in resources:
        exit_stack.callback(resource.close)
    exit_stack.close()


class _Camera(object):
    def __init__(self, name, mac):
        self.name = name
        self.mac_addr = mac
        self.id = None  # Remove as it's coupling with Mediaserver's API.

    def __repr__(self):
        return '<_Camera {} at {}>'.format(self.name, self.mac_addr)

    def get_info(self, parent_id):
        return make_camera_info(parent_id, self.name, self.mac_addr)


class CameraPool(object):
    """Emulates real QnCameraPool written from C++ Test Camera.

    Opens discovery UDP socket on 0.0.0.0 without SO_REUSEADDR,
    responds to local connections with `<camera_id>;<media_port>;<camera1_mac>;<camera2_mac>;...`.
    Discovery connections are accepted only from local IP addresses.
    (As VirtualBox, when UDP broadcast send from VM, shows that it comes from
    one of local interfaces. Admittedly, this is a little bit of coupling.)
    Media TCP server is opened on random free port,
    which is reported to mediaserver as <media_port>.
    """

    def __init__(self, stream_sample, discovery_port, media_port):
        self._termination_initiated = False
        self._socks = []  # type: List[_Interlocutor]
        self._media_sock = _MediaListener(media_port)
        self._socks.append(self._media_sock)
        self._camera_stream_sample = stream_sample
        self._discovery_sock = _DiscoveryUdpListener(discovery_port, self._media_sock.port)
        self._socks.append(self._discovery_sock)

    def __repr__(self):
        return '<CameraPool with {} and {}>'.format(self._discovery_sock, self._media_sock)

    @property
    def discovery_port(self):
        return self._discovery_sock.port

    def _select(self):  # type: () -> Tuple[List[_Interlocutor], List[_Interlocutor]]
        can_recv = [sock for sock in self._socks if sock.has_to_recv()]
        can_send = [sock for sock in self._socks if sock.has_to_send()]
        while True:
            _logger.debug("%r: select: %r, %r", self, can_recv, can_send)
            to_read, to_write, with_error = select.select(can_recv, can_send, can_recv + can_send, 0.1)
            if with_error:
                _logger.error("%r: sockets with error: %r", self, with_error)
            return to_read, to_write

    def _cleanup_ended(self):
        for sock in self._socks[:]:
            if sock.ended:
                sock.close()
                self._socks.remove(sock)

    def _collect_new_clients(self):
        self._socks.extend(
            _MediaStreamer(self._camera_stream_sample, sock)
            for sock in self._media_sock.new_clients)
        self._media_sock.new_clients[:] = []

    def add_camera(self, name, mac):
        self._discovery_sock.keys.append(mac)
        return _Camera(name, mac)

    def serve(self):
        _logger.info("%r: serve", self)
        while not self._termination_initiated:
            to_read, to_write = self._select()
            for sock in to_read:
                sock.recv()
            for sock in to_write:
                if not sock.ended:  # Same sock may come for read and write.
                    sock.send()
            self._cleanup_ended()
            self._collect_new_clients()

    def terminate(self):
        self._termination_initiated = True

    def close(self):
        _close_all(self._socks)


class _Interlocutor(object):
    def __init__(self, sock):
        self._sock = sock
        self._sock.setblocking(False)
        self.ended = False

    def __repr__(self):
        return '<{} at {}:{}>'.format(self.__class__.__name__, *self._sock.getsockname())

    @property
    def port(self):
        _, port = self._sock.getsockname()
        return port

    def fileno(self):
        return self._sock.fileno()

    def close(self):
        self._sock.close()

    def has_to_recv(self):
        return False

    def recv(self):
        pass

    def has_to_send(self):
        return False

    def send(self):
        pass


class _DiscoveryUdpListener(_Interlocutor):
    def __init__(self, port, media_port):
        super(_DiscoveryUdpListener, self).__init__(
            socket.socket(socket.AF_INET, socket.SOCK_DGRAM))
        self._sock.bind(('0.0.0.0', port))
        self._send_queue = deque()
        self._accepted_ips = [
            ip.ip
            for adapter in ifaddr.get_adapters()
            for ip in adapter.ips]
        self._media_port = media_port
        self.keys = []

    def has_to_recv(self):
        return True

    def recv(self):
        data, addr = self._sock.recvfrom(1024)
        if addr[0] not in self._accepted_ips:
            _logger.debug("%r: not from %r: %r", self, self._accepted_ips, addr[0])
            return
        if data != TEST_CAMERA_FIND_MSG:
            _logger.debug("%r: unknown find message from %r: %r", self, addr, data)
            return
        _logger.debug('%r: queue discovery response to %r', self, addr)
        self._send_queue.append(addr)

    def has_to_send(self):
        return bool(self._send_queue)

    def send(self):
        response = ';'.join([TEST_CAMERA_ID_MSG, str(self._media_port)] + self.keys)
        addr = self._send_queue[0]  # Leftmost queue element.
        _logger.info("%r: send discovery response to %r: %r", self, addr, response)
        self._sock.sendto(response, addr)
        self._send_queue.popleft()

    def fileno(self):
        return self._sock.fileno()


class _MediaListener(_Interlocutor):
    def __init__(self, media_port):
        super(_MediaListener, self).__init__(socket.socket(socket.AF_INET, socket.SOCK_STREAM))
        self._sock.bind(('0.0.0.0', media_port))
        self._sock.listen(20)  # 6 is used in one of the tests.
        self.new_clients = []

    def has_to_recv(self):
        return True

    def recv(self):
        client_sock, client_addr = self._sock.accept()
        _logger.info("%r: new media connection from %r", self, client_addr)
        self.new_clients.append(client_sock)


class _MediaStreamer(_Interlocutor):
    def __init__(self, camera_stream_sample, sock):
        super(_MediaStreamer, self).__init__(sock)
        self._camera_stream_sample = camera_stream_sample
        self._buffer = b''
        self._sent_up_to = timeit.default_timer() - self._camera_stream_sample.duration
        self._reload()

    def _reload(self):
        _logger.debug("%r: reload", self)
        assert not self._buffer
        self._buffer = memoryview(self._camera_stream_sample.data)
        self._sent_up_to += self._camera_stream_sample.duration

    def receive(self, buffer_size=64 * 1024):
        request = self._sock.recv(buffer_size)
        if request:
            _logger.info('%r: received request: %r', self, request)
        else:
            _logger.info('%r: connection closed from other side', self)
            self.ended = True

    def has_to_send(self):
        if self._buffer:
            _logger.debug("%r: has to send: buffer not empty", self)
            return True
        if 1 or self._sent_up_to < timeit.default_timer() - self._camera_stream_sample.duration:
            _logger.debug("%r: has to send: last sent too long ago")
            return True
        return False

    def send(self):
        _logger.debug('%r: send: %d bytes', self, len(self._buffer))
        try:
            bytes_sent = self._sock.send(self._buffer)
        except socket.error as e:
            if e.errno != errno.EPIPE:
                raise
            _logger.error('%r: connection closed from other side', self)
            self.ended = True
        else:
            _logger.debug("%r: sent: %d bytes", self, bytes_sent)
            assert bytes_sent > 0
            self._buffer = self._buffer[bytes_sent:]
            if not self._buffer:
                self._reload()


class SampleMediaFile(object):

    def __init__(self, path):
        parser = hachoir_parser.createParser(str(path).encode().decode())
        metadata = hachoir_metadata.extractMetadata(parser)
        self.path = path
        self.duration = metadata.get('duration')  # datetime.timedelta
        video_group = metadata['video[1]']
        self.width = video_group.get('width')
        self.height = video_group.get('height')

    def __repr__(self):
        return '<SampleMediaPath {!r} with duration {!r}>'.format(self.path, self.duration)


class SampleTestCameraStream(object):
    """Sample of video and data captured from Test Camera when streaming this file."""

    def __init__(self, sample_stream, duration):
        self.data = sample_stream
        self.duration = duration
