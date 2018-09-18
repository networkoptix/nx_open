import base64
import json
import pprint
import sys
import time
import queue
import threading
import logging
import pprint
from contextlib import contextmanager

from contextlib2 import ExitStack
import websocket

from .context_logger import ContextAdapter
from .utils import with_traceback

_logger = logging.getLogger(__name__)


class Transaction(object):

    def __init__(self, mediaserver_alias, command):
        self.mediaserver_alias = mediaserver_alias
        self.command = command

    def __str__(self):
        return 'Transaction from %s: %s' % (self.mediaserver_alias, self.command)


class _MessageBusThread(object):

    def __init__(self, mediaserver, queue, stop_flag):
        self._alias = mediaserver.api.generic.alias
        self._url = mediaserver.api.generic.http.base_url._replace(scheme='ws', path='/ec2/messageBus').url
        self._user = mediaserver.api.generic.http.user
        self._password = mediaserver.api.generic.http.password
        self._queue = queue
        self._stop_flag = stop_flag
        self._ws = None
        self._thread = threading.Thread(target=self._thread_main)
        self._logger = ContextAdapter(_logger.getChild('thread'), 'message-bus %s' % self._alias)

    def __repr__(self):
        return '<MessageBus %s @ %s:%s@%s>' % (self._alias, self._user, self._password, self._url)

    def start(self):
        self._ws = self._create_connection()
        self._thread.start()

    def join(self):
        self._thread.join()
        self._logger.info('Thread joined')
        if self._ws:
            self._ws.close()

    def _create_connection(self):
        credentials = '%s:%s' % (self._user, self._password)
        authorization = 'Basic ' + base64.b64encode(credentials.encode())
        headers = dict(Authorization=authorization)
        _logger.debug('Create websocket connection: %s', self._url)
        return websocket.create_connection(self._url, header=headers, timeout=0.5)

    @with_traceback
    def _thread_main(self):
        self._logger.info('Thread started')
        while not self._stop_flag:
            try:
                data = self._ws.recv()
                json_data = json.loads(data)
                self._logger.debug('Received:\n%s', pprint.pformat(json_data))
                transaction = Transaction(self._alias, command=json_data['tran']['command'])
                self._logger.info('Received: %s', transaction)
                self._queue.put(transaction)
            except websocket.WebSocketTimeoutException as x:
                pass
        self._logger.info('Thread finished')


class MessageBus(object):

    def __init__(self, mediaserver_iter):
        self._queue = queue.Queue()
        self._stop_flag = []
        self._bus_list = [_MessageBusThread(mediaserver, self._queue, self._stop_flag)
                              for mediaserver in mediaserver_iter]

    @contextmanager
    def running(self):
        self._start()
        try:
            yield self
        finally:
            self._stop()

    def _start(self):
        _logger.debug('Starting message buses.')
        stack = ExitStack()
        try:
            for bus in self._bus_list:
                bus.start()
                stack.callback(bus.join)
        except:
            self._stop_flag.append(None)
            stack.close()
            raise

    def _stop(self):
        _logger.debug('Stopping message buses:')
        self._stop_flag.append(None)
        for bus in self._bus_list:
            bus.join()
        _logger.debug('Stopping message buses: done')

    def get_transaction(self, timeout_sec):
        try:
            transaction = self._queue.get(timeout=timeout_sec)
            _logger.info('Message bus: %s', transaction)
            return transaction
        except queue.Empty:
            _logger.debug('Timed out waiting for transactions from message buses: timeout=%d sec', timeout_sec)
            return None

    def has_transactions(self, timeout_sec):
        return self.get_transaction(timeout_sec) is not None

    def wait_until_no_transactions(self, timeout_sec):
        while self.has_transactions(timeout_sec):
            pass
        _logger.info('Message bus: No transactions in %d sec.', timeout_sec)
