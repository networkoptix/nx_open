import collections
import contextlib
import datetime
import getpass
import json
import logging
import platform
import pprint
import select
import socket
import timeit
import uuid

import pytest
import tzlocal
from six.moves import http_client

import defaults

_logger = logging.getLogger(__name__)


@pytest.hookimpl()
def pytest_addoption(parser):
    parser.addoption(
        '--elasticsearch',
        metavar='HOSTNAME:PORT',
        help="If specified, upload logs (own and from Mediaserver) to Elasticsearch via HTTP API.",
        default=defaults.defaults.get('elasticsearch'))


@pytest.fixture(scope='session', autouse=True)
@pytest.mark.usefixtures('init_logging')  # TODO: Implement via hooks.
def _init_logging_to_elasticsearch(elasticsearch, metadata):
    if elasticsearch is None:
        return
    index = 'ft_runner'
    handler = _ElasticsearchLoggingHandler(elasticsearch, index, metadata)
    logging.getLogger().addHandler(handler)


@pytest.fixture(scope='session')
def elasticsearch(pytestconfig):
    addr_str = pytestconfig.getoption('--elasticsearch')
    if addr_str is None:
        return None
    return _ElasticsearchClient.from_str(addr_str)


def _json_default(o):
    if isinstance(o, datetime.datetime):
        return o.isoformat()
    return str(o)


_json_encoder = json.JSONEncoder(default=_json_default)


class _ElasticsearchClient(object):
    def __init__(self, addr):
        self.addr = addr
        now = datetime.datetime.now(tz=tzlocal.get_localzone())
        self.run_data = {'run': {'started_at': now, 'id': uuid.uuid1()}}

    @classmethod
    def from_str(cls, addr_str):
        hostname, port_str = addr_str.split(':')
        port = int(port_str)
        return cls((hostname, port))

    def connected_socket(self):
        s = socket.socket()
        # TCP buffer is used as queue. If there is a room for queue, `send` returns immediately.
        # Otherwise, the call blocks, and this is an intended behavior.
        s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1024 * 1024)
        _logger.debug("Connect to Elasticsearch at %s.", self.addr)
        s.connect(self.addr)
        return s

    @staticmethod
    def _make_chunk(data):
        bare = _json_encoder.encode(data).encode('ascii')
        return str(len(bare)).encode('ascii') + b'\r\n' + bare + b'\r\n'

    def _make_payload(self, static_data, items_iter):
        payload = bytearray()
        for item in items_iter:
            payload.extend(b'{"index":{}}\n')
            data = item.copy()
            data.update(static_data)
            data.update(self.run_data)
            payload.extend(_json_encoder.encode(data))
            payload.extend(b'\n')
        return payload

    def bulk_upload(self, index, static_data, items_iter):
        with contextlib.closing(self.connected_socket()) as s:
            _logger.info("Bulk upload to index %s with data: %r.", index, static_data)
            data = self._make_payload(static_data, items_iter)
            header_template = (
                'POST {index}/_doc/_bulk HTTP/1.1\r\n'
                'Connection: close\r\n'
                'Content-Type: application/x-ndjson\r\n'
                'Content-Length: {length}\r\n'
                '\r\n')
            s.send(header_template.format(index=index, length=len(data)))
            s.send(data)
            response = http_client.HTTPResponse(s)
            response.begin()
            response_payload = response.read()
            if not 200 <= response.status != 299:
                _logger.debug(
                    "Status: %d, response:\n%s",
                    response.status,
                    pprint.pformat(json.loads(response_payload)))
            assert s.recv(1) == b''
            s.shutdown(socket.SHUT_WR)


class _ElasticsearchLoggingHandler(logging.Handler):
    def __init__(self, client, index, app):
        super(_ElasticsearchLoggingHandler, self).__init__()

        self._sent_count = 0
        self._accepted_count = 0
        self._rejected_count = 0

        self._tz = tzlocal.get_localzone()

        self._static = {
            'app': app,
            'system': {
                'hostname': socket.gethostname(),
                'username': getpass.getuser(),
                'platform': platform.platform(),
                'python': platform.python_version(),
                },
            }
        self._static.update(client.run_data)

        self._sock = client.connected_socket()
        self._index = index

        self._response_processor = self._process_responses_stream()
        self._response_processor.next()  # Run until first `yield`.

    def _log_stats(self):
        _logger.debug(
            "Sent: %d; accepted: %d; rejected: %d.",
            self._sent_count, self._accepted_count, self._rejected_count)

    def _process_responses_stream(self):
        queue = collections.deque()
        next_stats_report = timeit.default_timer()
        while True:
            response = http_client.HTTPResponse(self._sock, buffering=False)
            # ElasticSearch usually responds with two packets: headers and content.
            # Therefore, it's very unlikely that reading blocks.
            while select.select([self._sock], [], [self._sock], 0) == ([], [], []):
                queue.extend((yield))
            response.begin()
            if response.getheader(b'Content-Length') == b'0':
                payload = response.read()
                assert not payload
                _logger.debug("Got response to OPTIONS with Connection: close.")
                break
            if timeit.default_timer() > next_stats_report:
                self._log_stats()
                next_stats_report += 0.5
            while select.select([self._sock], [], [], 0) == ([], [], []):
                queue.extend((yield))
            payload = response.read()
            assert payload
            assert response.isclosed()
            if not 200 <= response.status <= 299:
                self._rejected_count += 1
                _logger.error(
                    "Rejected:\n%s\n%s",
                    pprint.pformat(queue[0]),
                    pprint.pformat(json.loads(payload)))
            else:
                self._accepted_count += 1
            queue.popleft()

    def emit(self, record):
        # noinspection PyBroadException
        try:
            if record.name == _logger.name:
                return
            data = {
                field: record.__dict__[field]
                for field in record.__dict__
                # Args frequently cause errors on indexing. Some other fields are redundant.
                if field not in ('args', 'msg', 'created', 'msec', 'asctime')}
            data['ts'] = datetime.datetime.fromtimestamp(record.created, tz=self._tz),
            data.update(self._static)
            payload = _json_encoder.encode(data).encode('ascii')
            # In HTTP 1.1 Connection: keep-alive is the default.
            request = bytearray()
            request.extend('POST {}/_doc HTTP/1.1\r\n'.format(self._index).encode('ascii'))
            request.extend('Content-Type: application/json\r\n'.encode('ascii'))
            request.extend('Content-Length: {}\r\n'.format(len(payload)).encode('ascii'))
            request.extend(b'\r\n')
            request.extend(payload)
            self._sent_count += 1
            self._response_processor.send([request])
            # Sending must be the single call and must be called last, so all this method can be
            # interpreted as a whole critical section under GIL.
            self._sock.send(request)
        except Exception:
            self.handleError(record)

    def close(self):
        _logger.debug("Ask to close connection.")
        request = bytearray()
        request.extend('OPTIONS {}/_doc HTTP/1.1\r\n'.format(self._index).encode('ascii'))
        request.extend(b'Connection: close\r\n')
        request.extend(b'\r\n')
        self._sock.send(request)
        _logger.debug("Read out.")
        try:
            while True:
                self._response_processor.send([])
        except StopIteration:
            pass
        self._log_stats()
        _logger.debug("Close.")
        self._sock.close()


def test_handler(elasticsearch):
    logger = logging.getLogger('__test')
    logger.setLevel(logging.DEBUG)
    addr = 'localhost', 9200
    handler = _ElasticsearchLoggingHandler(_ElasticsearchClient(addr), 'test_handler', {'a': 1})
    try:
        logger.addHandler(handler)
        entries = collections.deque(
            {
                'number': i * i,
                'string': 'value{:010d}'.format(i),
                'dict': {'a': i, 'b': i * i},
                'list': [i, i * i],
                'json': json.dumps({'x': i, 'y': i * i}),
                'optional_' + 'ab'[i % 2]: 'dummy',
                'maybe_null': [None, False, True][i % 3],
                }
            for i in range(2000)
            )
        while entries:
            entry = entries[0]
            logger.debug("Dummy message.", extra=entry)
            entries.popleft()
    finally:
        handler.close()


def test_bulk_upload(elasticsearch):
    items = ({'number': i, 'ts': '2018-10-10T10:10:{:02d}.000Z'.format(i % 60)} for i in range(2000))
    started_at = datetime.datetime.now(tz=tzlocal.get_localzone())
    static_data = {'run': {'id': uuid.uuid1(), 'started_at': started_at}}
    elasticsearch.bulk_upload('test_bulk_upload', static_data, items)


if __name__ == '__main__':
    _logger.addHandler(logging.StreamHandler())
    _logger.setLevel(logging.DEBUG)
    elasticsearch_client = _ElasticsearchClient(('localhost', 9200))
    test_handler(elasticsearch_client)
    test_bulk_upload(elasticsearch_client)
