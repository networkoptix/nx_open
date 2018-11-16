from collections import namedtuple
import errno
import json
import pprint
import sys
import time
import queue as queue_module
import socket
import threading
import logging
from contextlib import contextmanager

from contextlib2 import ExitStack
import websocket

from .context_logger import ContextAdapter
from .threaded import ThreadedCall
from .utils import with_traceback, datetime_local_now

_logger = logging.getLogger(__name__)


class TransactionCommand(object):

    def __init__(self, name, data):
        self.name = name

    def __str__(self):
        return self.name


class SetResourceParamCommand(TransactionCommand):

    _Param = namedtuple('SetResourceParamCommand_Param', 'name resourceId value')

    def __init__(self, name, data):
        super(SetResourceParamCommand, self).__init__(name, data)
        params = data['tran']['params']
        self.param = self._Param(
            name=params['name'],
            resourceId=params['resourceId'],
            value=params['value'],
            )

    def __str__(self):
        return '%s: %s/%s = %r' % (self.name, self.param.name, self.param.resourceId, self.param.value)


command_name_to_class = dict(
    setResourceParam=SetResourceParamCommand,
    )


class Transaction(object):

    def __init__(self, mediaserver_alias, received_at, command):
        self.mediaserver_alias = mediaserver_alias
        self.received_at = received_at  # type: datetime
        self.command = command  # type: TransactionCommand

    def __str__(self):
        return 'Transaction from %s: %s' % (self.mediaserver_alias, self.command)


class _MessageBusQueue(object):

    def __init__(self, queue):
        self._queue = queue

    def get_transaction(self, timeout_sec):
        try:
            transaction = self._queue.get(timeout=timeout_sec)
            _logger.info('Message bus: %s', transaction)
            return transaction
        except queue_module.Empty:
            _logger.debug('Timed out waiting for transactions from message buses: timeout=%d sec', timeout_sec)
            return None

    def wait_until_no_transactions(self, timeout_sec):
        while self.get_transaction(timeout_sec) is not None:
            pass
        _logger.info('Message bus: No transactions in %d sec.', timeout_sec)


@contextmanager
def _bus_thread_running(server, queue, socket_reopened_counter):

    def open_websocket():
        return server.api.generic.http.open_websocket('/ec2/messageBus')

    logger = ContextAdapter(_logger.getChild('thread'), 'message-bus %s' % server.api.generic.alias)
    wsl = [open_websocket()]

    def reopen_websocket():
        logger.info('Reopening socket.')
        wsl[0].close()
        wsl[0] = open_websocket()
        if socket_reopened_counter:
            socket_reopened_counter.incr()

    def parse_transaction(json_data, received_at):
        data = json.loads(json_data)
        logger.debug('Received:\n%s', pprint.pformat(data))
        command_name = data['tran']['command']
        command_class = command_name_to_class.get(command_name, TransactionCommand)
        command = command_class(command_name, data)
        transaction = Transaction(server.api.generic.alias, received_at, command)
        logger.info('Received: %s', transaction)
        return transaction

    def process_transaction():
        try:
            json_data = wsl[0].recv()
        except websocket.WebSocketTimeoutException as x:
            pass
        except websocket.WebSocketConnectionClosedException as x:
            logger.error('WebSocket connection closed: %s', x)
            reopen_websocket()
        except socket.error as x:
            logger.error('Socket error: [%s] %s', x.errno, x.strerror)
            if x.errno in [errno.ECONNRESET, errno.EPIPE]:
                reopen_websocket()
            else:
                raise
        else:
            received_at = datetime_local_now()
            queue.put(parse_transaction(json_data, received_at))

    try:
        with ThreadedCall.periodic(process_transaction, description='message-bus', logger=logger):
            yield
    finally:
        wsl[0].close()


@contextmanager
def message_bus_running(mediaserver_iter, socket_reopened_counter=None):
        # type: (Iterable[Mediaserver], ThreadSafeCounter) -> Generator[_MessageBusQueue]
    queue = queue_module.Queue()
    # Stop flag is shared between threads for faster shutdown:
    # after first thread's yield returned all other will start exiting too.
    with ExitStack() as stack:
        _logger.debug('Starting message bus threads.')
        for server in mediaserver_iter:
            stack.enter_context(_bus_thread_running(server, queue, socket_reopened_counter))
        yield _MessageBusQueue(queue)
        _logger.debug('Stopping message bus threads:')
    _logger.debug('Stopping message bus threads: done')
