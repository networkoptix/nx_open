import logging
import math
import struct
import time
import urllib
import urlparse
from datetime import datetime, timedelta

import cv2
import requests
from requests.auth import HTTPDigestAuth

from .artifact import ArtifactType
from .utils import datetime_utc_to_timestamp

_logger = logging.getLogger(__name__)


CHUNK_SIZE = 1024*100  # bytes
RTSP_SPEED = 4  # maximum is 32, but only 4 is working with opencv shipped with ubuntu 14.04
AVI_ARTIFACT_TYPE = ArtifactType(name='video-avi', content_type='video/avi', ext='.avi')
WEBM_ARTIFACT_TYPE = ArtifactType(name='video-webm', content_type='video/webm', ext='.webm')
MKV_ARTIFACT_TYPE = ArtifactType(name='video-mkv', content_type='video/x-matroska', ext='.mkv')
MPEG_ARTIFCT_TYPE = ArtifactType(name='video-mpeg', content_type='video/mpeg', ext='.mpeg')


class Metadata(object):

    @classmethod
    def from_file(cls, file_path):
        assert file_path.stat().st_size != 0, 'Mediaserver returned empty media stream'
        cap = cv2.VideoCapture(str(file_path))
        assert cap.isOpened(), 'Media stream returned from server is invalid (saved to %r)' % file_path
        try:
            metadata = cls(cap)
            metadata.log_properties(file_path)
            return metadata
        finally:
            cap.release()

    def __init__(self, cap):
        self.frame_count = cap.get(cv2.cv.CV_CAP_PROP_FRAME_COUNT)
        self.width = int(cap.get(cv2.cv.CV_CAP_PROP_FRAME_WIDTH))
        self.height = int(cap.get(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT))
        self.fps = cap.get(cv2.cv.CV_CAP_PROP_FPS)
        self.fourcc = int(cap.get(cv2.cv.CV_CAP_PROP_FOURCC))

    def log_properties(self, name):
        _logger.info('media stream %r properties:', name)
        _logger.info('\tframe count: %s', self.frame_count)
        _logger.info('\tfps: %s', self.fps)
        _logger.info('\twidth: %d', self.width)
        _logger.info('\theight: %d', self.height)
        _logger.info('\tfourcc: %r (%d) ', struct.pack('I', self.fourcc), self.fourcc)


class RtspMediaStream(object):

    def __init__(self, server_url, user, password, camera_mac_addr):
        params = dict(pos=0, speed=RTSP_SPEED)
        self.url = 'rtsp://{user}:{password}@{netloc}/{camera_mac_addr}?{params}'.format( 
            user=user,
            password=password,
            netloc=urlparse.urlparse(server_url).netloc,
            camera_mac_addr=camera_mac_addr,
            params=urllib.urlencode(params),
            )
        self.user = user
        self.password = password

    def load_archive_stream_metadata(self, artifact_factory, pos=None, duration=None):
        temp_file_path = artifact_factory(name='media-avi', artifact_type=AVI_ARTIFACT_TYPE).produce_file_path()
        _logger.info('RTSP request: %r', self.url)
        from_cap = cv2.VideoCapture(self.url)
        assert from_cap.isOpened(), 'Failed to open RTSP url: %r' % self.url
        try:
            self._write_cap_to_file(from_cap, temp_file_path)
        finally:
            from_cap.release()
        return [Metadata.from_file(temp_file_path)]

    def _write_cap_to_file(self, from_cap, temp_file_path):
        start_time = time.time()
        metadata = Metadata(from_cap)
        metadata.log_properties(self.url)
        fps = metadata.fps
        if math.isnan(fps):
            fps = 15  # Use at least something if we are unable to get it from opencv
        to_cap = cv2.VideoWriter(str(temp_file_path), metadata.fourcc, fps, (metadata.width, metadata.height))
        assert to_cap.isOpened(), 'Failed to open OpenCV media writer to file %r' % temp_file_path
        try:
            frame_count = self._copy_cap(from_cap, to_cap, start_time)
        finally:
            to_cap.release()
        size = temp_file_path.stat().st_size
        _logger.info(
            'RTSP stream: completed loading %d frames in %.2f seconds, total size: %dB/%.2fKB/%.2fMB',
            frame_count, time.time() - start_time, size, size/1024., size/1024./1024)

    def _copy_cap(self, from_cap, to_cap, start_time):
        frame_count = 0
        log_time = time.time()
        while True:
            ret, frame = from_cap.read()
            if not ret:
                break
            to_cap.write(frame)
            frame_count += 1
            t = time.time()
            if t - log_time >= 5:  # log every 5 seconds
                msec = from_cap.get(cv2.cv.CV_CAP_PROP_POS_MSEC)
                _logger.debug(
                    'RTSP stream: loaded %d frames in %.2f seconds, current position: %.2f seconds',
                    frame_count, t - start_time, msec/1000.)
                log_time = t
        return frame_count


def load_stream_metadata_from_http(stream_type, url, user, password, params, temp_file_path):
    _logger.info('%s request: %r, user=%r, password=%r, %s',
             stream_type.upper(), url, user, password, ', '.join(str(p) for p in params))
    response = requests.get(url, auth=HTTPDigestAuth(user, password), params=params, stream=True)
    return load_stream_metadata_from_http_response(stream_type, response, temp_file_path)


def load_stream_metadata_from_http_response(stream_type, response, temp_file_path):
    _logger.info('%s response: [%d] %s', stream_type.upper(), response.status_code, response.reason)
    for name, value in response.headers.items():
        _logger.debug('\tresponse header: %s: %s', name, value)
    response.raise_for_status()
    start_time = log_time = time.time()
    chunk_count = 0
    size = 0
    with temp_file_path.open('wb') as f:
        for chunk in response.iter_content(chunk_size=CHUNK_SIZE):
            f.write(chunk)
            size += len(chunk)
            chunk_count += 1
            t = time.time()
            if t - log_time >= 5:  # log every 5 seconds
                _logger.debug('%s stream: loaded %d bytes in %d chunks', stream_type.upper(), size, chunk_count)
                log_time = t
    _logger.info('%s stream: completed loading %d chunks in %.2f seconds, total size: %dB/%.2fKB/%.2fMB',
             stream_type.upper(), chunk_count, time.time() - start_time, size, size/1024., size/1024./1024)
    return Metadata.from_file(temp_file_path)


class WebmMediaStream(object):

    def __init__(self, server_url, user, password, camera_mac_addr):
        self.url = '%smedia/%s.webm' % (server_url, camera_mac_addr)
        self.user = user
        self.password = password

    def load_archive_stream_metadata(self, artifact_factory, pos=None, duration=None):
        temp_file_path = artifact_factory(name='media-webm', artifact_type=WEBM_ARTIFACT_TYPE).produce_file_path()
        params = dict(pos=0)
        metadata = load_stream_metadata_from_http(
            'webm', self.url, self.user, self.password, params, temp_file_path)
        return [metadata]


class DirectHlsMediaStream(object):

    def __init__(self, server_url, user, password, camera_mac_addr):
        self.url = '%shls/%s.mkv' % (server_url, camera_mac_addr)
        self.user = user
        self.password = password

    def load_archive_stream_metadata(self, artifact_factory, pos, duration):
        assert isinstance(pos, datetime), repr(pos)
        assert isinstance(duration, timedelta), repr(duration)
        temp_file_path = artifact_factory(name='media-mkv', artifact_type=MKV_ARTIFACT_TYPE).produce_file_path()
        pos_ms = int(datetime_utc_to_timestamp(pos) * 1000)
        duration_sec = int(duration.total_seconds() + 1)  # round to next value
        params = dict(pos=pos_ms, duration=duration_sec)
        metadata = load_stream_metadata_from_http(
            'hls', self.url, self.user, self.password, params, temp_file_path)
        return [metadata]


class M3uHlsMediaStream(object):

    def __init__(self, server_url, user, password, camera_mac_addr):
        self.server_url = server_url
        self.user = user
        self.password = password
        self.camera_mac_addr = camera_mac_addr

    def load_archive_stream_metadata(self, artifact_factory, pos=None, duration=None):
        loader = M3uHlsMediaMetainfoLoader(self.server_url, self.user, self.password, artifact_factory)
        return loader.run(self.camera_mac_addr)


class M3uHlsMediaMetainfoLoader(object):

    def __init__(self, server_url, user, password, artifact_factory):
        self.server_url = server_url
        self.user = user
        self.password = password
        self.artifact_factory = artifact_factory
        self.collected_metainfo_list = []
        self.file_counter = 0

    def run(self, camera_mac_addr):
        url = '%shls/%s.m3u' % (self.server_url, camera_mac_addr)
        self._make_hls_request(url)
        return self.collected_metainfo_list

    def _make_hls_request(self, url):
        _logger.info('HLS request: %r, user=%r, password=%r', url, self.user, self.password)
        params = dict(pos=0)
        response = requests.get(
            url, auth=HTTPDigestAuth(self.user, self.password), params=params, stream=True)
        _logger.info('HLS response: [%d] %s', response.status_code, response.reason)
        for name, value in response.headers.items():
            _logger.debug('\tresponse header: %s: %s', name, value)
        response.raise_for_status()
        content_type = response.headers['Content-Type']
        if content_type == 'audio/mpegurl':
            self._process_url_response(response)
        else:
            self._process_media_response(response)

    def _process_url_response(self, response):
        paths = [line for line in response.content.splitlines() if line and not line.startswith('#')]
        for path in paths:
            _logger.debug('HLS: received path %r' % path)
        for path in paths:
            self._make_hls_request(self.server_url.rstrip('/') + path)

    def _process_media_response(self, response):
        self.file_counter += 1
        artifact_factory = self.artifact_factory(
            [str(self.file_counter)], name='media-%d-mpeg' % self.file_counter, artifact_type=MPEG_ARTIFCT_TYPE)
        temp_file_path = artifact_factory.produce_file_path()
        metadata = load_stream_metadata_from_http_response('hls', response, temp_file_path)
        self.collected_metainfo_list.append(metadata)
