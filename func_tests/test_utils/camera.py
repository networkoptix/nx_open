'''Camera support classes

Server has separate protocol created specifically for test cameras. It multicast UDP packets on port 4984
and expects UDP responses from test cameras, with camera mac address and TCP endpoint for media streaming.
Then it connects to that endpoint using TCP, with one-line request and expects media stream with specific
formatting in response.
All this is supported by 3 classes:
* DiscoveryUdpListener - listens and responds to UDP requets
* MediaListener - listens on TCP port to receive media stream requests
* MediaStreamer - reads TCP request on connected socket and sends media stream from file.
All these 3 classes are internal for this module; Tests only see and use Camera and CameraFactory instances,
created using 'camera' or 'camera_factory' fixtures.
'''

import datetime
import logging
import select
import socket
import threading
import time

import hachoir_core.config
import pytest

# overwise hachoir will replace sys.stdout/err with UnicodeStdout, incompatible with pytest terminal module:
hachoir_core.config.unicode_stdout = False
import hachoir_parser
import hachoir_metadata
from .utils import datetime_utc_now

log = logging.getLogger(__name__)
        

DEFAULT_CAMERA_MAC_ADDR = '11:22:33:44:55:66'
TEST_CAMERA_NAME = 'TestCameraLive'  # hardcoded to server, mandatory for auto-discovered cameras

CAMERA_DISCOVERY_WAIT_TIMEOUT = datetime.timedelta(minutes=1)

TEST_CAMERA_DISCOVERY_PORT = 4984  # hardcoded to server UDP multicast address for test camera
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
        #model=name,
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
        #typeId='{7d2af20d-04f2-149f-ef37-ad585281e3b7}',
        typeId='{f9c03047-72f1-4c04-a929-8538343b6642}',
        url='127.0.0.100',
        vendor='python-funtest',
        )

def make_schedule_task(day_of_week):
    return dict(
        afterThreshold=5,
        beforeThreshold=5,
        dayOfWeek=day_of_week,
        endTime=86400,
        fps=15,
        recordAudio=False,
        recordingType="RT_Always",
        startTime=0,
        streamQuality="high",
        )


class Camera(object):

    def __init__(self, vm_address, discovery_listener, name, mac_addr):
        self._vm_address = vm_address  # IPAddress or None
        self._discovery_listener = discovery_listener
        self.name = name
        self.mac_addr = mac_addr
        self.id = None  # camera guid on server, set when registered on server

    def __str__(self):
        return '%s at %s' % (self.name, self.mac_addr)

    def __repr__(self):
        return 'Camera(%s)' % self

    def get_info(self, parent_id):
        return make_camera_info(parent_id, self.name, self.mac_addr)

    def wait_until_discovered_by_server(self, server_list, timeout=CAMERA_DISCOVERY_WAIT_TIMEOUT):
        #assert is_list_inst(server_list, Server), repr(server_list)
        log.info('Waiting for camera %s to be discovered by servers %s', self, ', '.join(map(str, server_list)))
        start_time = datetime_utc_now()
        while datetime_utc_now() - start_time < timeout:
            for server in server_list:
                for d in server.rest_api.ec2.getCamerasEx.GET():
                    if d['physicalId'].replace('-', ':') == self.mac_addr:
                        self.id = d['id']
                        log.info('Camera %s is discovered by server %s, registered with id %r', self, server, self.id)
                        return
            time.sleep(5)
        pytest.fail('Camera %s was not discovered by servers %s in %s'
                    % (self, ', '.join(map(str, server_list)), CAMERA_DISCOVERY_WAIT_TIMEOUT))

    def switch_to_server(self, server):
        assert self.id, 'Camera %s is not yet registered on server' % self
        server.rest_api.ec2.saveCamera.POST(id=self.id, parentId=server.ecs_guid)
        for d in server.rest_api.ec2.getCamerasEx.GET():
            if d['id'] == self.id:
                break
        else:
            pytest.fail('Camera %s is unknown for server %s' % (self, server))
        assert d['parentId'] == server.ecs_guid

    def start_streaming(self):
        # assert isinstance(server, Server), repr(server)  # import circular dependency
        self._discovery_listener.stream_to(self, self._vm_address)


class CameraFactory(object):

    def __init__(self, vm_address, media_stream_path):
        self._vm_address = vm_address
        self._discovery_listener = DiscoveryUdpListener(media_stream_path)

    def __call__(self, name=TEST_CAMERA_NAME, mac_addr=DEFAULT_CAMERA_MAC_ADDR):
        return Camera(self._vm_address, self._discovery_listener, name, mac_addr)

    def close(self):
        self._discovery_listener.stop()


class DiscoveryUdpListener(object):

    def __init__(self, media_stream_path):
        self._media_stream_path = media_stream_path
        self._stream_to_camera_list = []  # Camera list
        self._stream_to_address_list = []  # string list
        self._thread = None
        self._stop_flag = False
        self._media_listeners = []

    def stop(self):
        self._stop_flag = True
        if self._thread:
            log.debug('Waiting for test camera UDP discovery listener to stop...')
            self._thread.join()
        for listener in self._media_listeners:
            listener.stop()
        log.info('Test camera UDP discovery listener is stopped.')

    def stream_to(self, camera, ip_address):
        log.info('Test camera %s: will stream to %s', camera.name, ip_address)
        self._stream_to_camera_list.append(camera)
        self._stream_to_address_list.append(str(ip_address) if ip_address else None)
        if not self._thread:
            self._start()

    def _start(self):
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        host = '0.0.0.0'
        port = TEST_CAMERA_DISCOVERY_PORT
        listen_socket.bind((host, port))
        log.info('Test camera discoverer: UDP Listening on %s:%d', host, port)
        self._thread = threading.Thread(target=self._thread_main, args=(listen_socket,))
        self._thread.daemon = True
        self._thread.start()

    def _thread_main(self, listen_socket):
        log.info('Test camera UDP discovery listener thread is started.')
        while not self._stop_flag:
            read, write, error = select.select([listen_socket], [], [listen_socket], 0.1)
            if not read and not error:
                continue
            data, addr = listen_socket.recvfrom(1024)
            # log.debug('Received discovery message from %s:%d: %r', addr[0], addr[1], data)
            if data != TEST_CAMERA_FIND_MSG:
                continue
            if None not in self._stream_to_address_list and addr[0] not in self._stream_to_address_list:
                continue  # request came not from our server
            for camera in self._stream_to_camera_list:
                listener = MediaListener(self._media_stream_path)
                response = '%s;%d;%s' % (TEST_CAMERA_ID_MSG, listener.port, camera.mac_addr)
                log.info('Responding to %s:%d: %r', addr[0], addr[1], response)
                listen_socket.sendto(response, addr)
                self._media_listeners.append(listener)
        listen_socket.close()
        log.debug('Test camera UDP discovery listener thread is finished.')


class MediaListener(object):

    def __init__(self, media_stream_path):
        self._media_stream_path = media_stream_path
        self._stop_flag = False
        self._streamers = []
        listen_host = '0.0.0.0'
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.bind((listen_host, 0))
        listen_socket.listen(5)
        self.host, self.port = listen_socket.getsockname()
        self._thread = threading.Thread(target=self._thread_main, args=(listen_socket,))
        self._thread.daemon = True
        self._thread.start()

    def __str__(self):
        return ('Test camera media listener at %s:%d' % (self.host, self.port))

    def stop(self):
        log.info('%s with %d active streamers: stopping...', self, len(self._streamers))
        self._stop_flag = True
        self._thread.join()
        for streamer in self._streamers:
            streamer.stop()
        log.info('%s: stopped.', self)

    def _thread_main(self, listen_socket):
        log.info('%s: thread started.', self)
        while not self._stop_flag:
            read, write, error = select.select([listen_socket], [], [listen_socket], 0.1)
            if not read and not error:
                continue
            sock, addr = listen_socket.accept()
            log.info('%s: accepted connection from %s:%d', self, addr[0], addr[1])
            streamer = MediaStreamer(self._media_stream_path, sock, addr)
            self._streamers.append(streamer)
        listen_socket.close()
        log.info('%s: thread is finished.', self)


class MediaStreamer(object):

    def __init__(self, media_stream_path, sock, peer_address):
        self._media_stream_path = media_stream_path
        self._peer_address = peer_address
        self._stop_flag = False
        self._thread = threading.Thread(target=self._thread_main, args=(sock,))
        self._thread.isDaemon = True
        self._thread.start()

    def __str__(self):
        host, port = self._peer_address
        return ('Test camera media streamer for %s:%d' % (host, port))

    def stop(self):
        self._stop_flag = True
        self._thread.join()
        log.info('%s: stopped.', self)

    def _thread_main(self, sock):
        try:
            self._run(sock)
        except socket.error as x:
            log.info('%s is finished: %r', self, x)

    def _run(self, sock):
        request = sock.recv(1024)            
        log.info('%s: received request %r; starting streaming %s', self, request, self._media_stream_path)
        while not self._stop_flag:
            with open(self._media_stream_path, 'rb') as f:
                while True:
                    data = f.read(1024)
                    if not data:
                        break
                    #log.debug('%s: sending data, %d bytes', self, len(data))
                    sock.sendall(data)
                    if self._stop_flag:
                        break
        sock.close()
        log.debug('%s: thread is finished.', self)


class SampleMediaFile(object):

    def __init__(self, fpath):
        metadata = self._read_metadata(fpath)
        self.fpath = fpath
        self.duration = metadata.get('duration')  # datetime.timedelta
        video_group = metadata['video[1]']
        self.width = video_group.get('width')
        self.height = video_group.get('height')

    def __repr__(self):
        return 'SampleMediaPath(%r, duration=%s)' % (self.fpath, self.duration)

    def _read_metadata(self, fpath):
        parser = hachoir_parser.createParser(unicode(fpath))
        return hachoir_metadata.extractMetadata(parser)

    def get_contents(self):
        with open(self.fpath, 'rb') as f:
            return f.read()
