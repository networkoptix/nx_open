#!/usr/bin/env python2

# Generate/post test transactions on servers with requested rate.

import argparse
from contextlib import contextmanager
from datetime import timedelta
from functools import partial
import itertools
import logging
import time
import threading

from contextlib2 import ExitStack

from framework.context_logger import context_logger
from framework.threaded import ThreadedCall
from framework.utils import (
    datetime_local_now,
    FunctionWithDescription,
    imerge,
    threadsafe_generator,
)
from metrics import MetricsCollector
from existing_mediaservers import address_and_count_to_server_list
from cmdline_logging import init_logging


_logger = logging.getLogger('scalability.transaction_generator')
_init_logger = _logger.getChild('init')
post_stamp_logger = _logger.getChild('stamp')


POST_TRANSACTION_GENERATOR_THREAD_COUNT = 16
STALE_LIMIT_RATIO = 0.1


class _CameraInfo(object):

    def __init__(self, camera_id, is_online=False):
        self.id = camera_id
        self.is_online = is_online

    def toggle_status(self):
        self.is_online = not self.is_online

    @property
    def status(self):
        if self.is_online:
            return 'Online'
        else:
            return 'Offline'


@threadsafe_generator
def transaction_generator(server_list):

    def server_generator(server):
        with context_logger(post_stamp_logger, 'framework.http_api'):
            with context_logger(post_stamp_logger, 'framework.mediaserver_api'):
                server_id = server.api.get_server_id()
                camera_list = [_CameraInfo(d['id'])
                               for d in server.api.generic.get('ec2/getCameras')
                               if d['parentId'] == server_id]
        if not camera_list:
            # This may happen if we use lightweight servers and this is full one.
            # Full server does not has it's own cameras in this case.
            return
        for i, camera in enumerate(camera_list):
            camera.is_online = bool(i % 2)

        for idx in itertools.count():

            # change every camera status
            for camera_idx, camera in enumerate(camera_list):
                camera.toggle_status()
                description = 'to %s: status for #%d %s: %s' % (
                    server.name, camera_idx, camera.id, camera.status)
                params = dict(
                    id=camera.id,
                    status=camera.status,
                    )
                yield FunctionWithDescription(
                    partial(server.api.generic.post, 'ec2/setResourceStatus', params),
                    description,
                    )

            # set timestamp for one of cameras
            camera_idx = idx % len(camera_list)
            camera = camera_list[camera_idx]
            iso_datetime = datetime_local_now().isoformat()
            value = '{}.{:02}.{}'.format(server.name, camera_idx, iso_datetime)
            param_list = [dict(
                resourceId=camera.id,
                name='scalability-stamp',
                value=value,
                )]
            yield FunctionWithDescription(
                partial(server.api.generic.post, 'ec2/setResourceParams', param_list),
                'to %s: stamp for #%d %s: %s' % (server.name, camera_idx, camera.id, value),
                )

    return imerge(*(server_generator(server) for server in server_list))


class _Pacer(object):
    """Ensure calls from multiple threads issued at required rate"""

    INTERVAL = 10  # seconds; check and recalculate delays per INTERVAL seconds

    def __init__(self, required_rate, thread_count, metrics_collector):
        assert required_rate, repr(required_rate)  # must not be zero
        assert thread_count, repr(thread_count)  # must not be zero
        self._required_interval_rate = required_rate * self.INTERVAL  # transactions per interval
        self._thread_count = thread_count
        self._metrics_collector = metrics_collector
        self._lock = threading.Lock()
        self._interval_start = time.time()
        self._transaction_count = 0  # during last interval
        self._transactions_post_time = 0.  # accumulated time for transaction posting
        self._sleep_time = 0.
        _logger.debug('Pacer: interval=%s required_interval_rate=%s',
                      self.INTERVAL, self._required_interval_rate)
        
    @contextmanager
    def pace_call(self):
        t = time.time()
        yield
        self._transactions_post_time += time.time() - t

        time.sleep(self._sleep_time)

        with self._lock:
            self._transaction_count += 1
            if self._transaction_count < self._required_interval_rate:
                return
            # now we have posted all transactions required for our interval (1sec)
            thread_post_time = self._transactions_post_time / self._thread_count
            remaining_time = self.INTERVAL - thread_post_time
            if thread_post_time < self.INTERVAL:
                self._sleep_time = remaining_time * self._thread_count / self._transaction_count
                time_left = self.INTERVAL - (time.time() - self._interval_start)
                if time_left > 0:
                    _logger.debug('%.03s seconds is left to the end of this interval; sleeping.', time_left)
                    time.sleep(time_left)  # sleep rest of this interval, blocking other threads
                stale_time = 0
            else:
                self._sleep_time = 0
                stale_time = thread_post_time - self.INTERVAL
                _logger.warning(
                    ('Servers unable to process transactions at required rate (%s/%s sec),'
                     ' stalled for %s seconds') % (self._required_interval_rate, self.INTERVAL, stale_time))
            self._metrics_collector.add_stale_time(stale_time)
            _logger.debug('Interval stats:'
                          + ' transactions_post_time=%s' % self._transactions_post_time
                          + ' remaining_time=%s' % remaining_time
                          + ' stale_time=%s' % stale_time
                          + ' new sleep_time=%s' % self._sleep_time
                          )
            self._interval_start = time.time()
            self._transactions_post_time = 0.
            self._transaction_count = 0


@contextmanager
def transactions_posted(
        metrics_collector, call_generator, rate, thread_count, stop_event, exception_event):
    pacer = _Pacer(rate, thread_count, metrics_collector)

    def issue_transaction():
        context_1 = context_logger(post_stamp_logger, 'framework.http_api')
        context_2 = context_logger(post_stamp_logger, 'framework.mediaserver_api')
        with pacer.pace_call(), context_1, context_2:
            fn = next(call_generator)
            post_stamp_logger.info('Post %s', fn.description)
            t = time.time()
            fn()
            metrics_collector.add_post_duration(time.time() - t)
            metrics_collector.incr_posted_transaction_counter()

    stop_event = threading.Event()  # stop all threads on exception in any one
    with ExitStack() as stack:
        _logger.debug('Starting transaction generating threads.')
        for idx in range(thread_count):
            stack.enter_context(ThreadedCall.periodic(
                issue_transaction,
                join_timeout_sec=30,
                description='post-{}'.format(idx),
                stop_event=stop_event,
                exception_event=exception_event,
                ))

        yield

        _logger.debug('Stopping transaction generating threads:')
    _logger.debug('Stopping transaction generating threads: done.')


def _generation_main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logging-config', default='default.yaml',
                        help=("Configuration file for logging, in yaml format."
                              " Relative to logging-config dir if relative."))
    parser.add_argument('--verbose', '-v', action='store_true', help="Verbose logging.")
    parser.add_argument('--lws', action='store_true',
                        help="Use lightweight servers instead of full ones.")
    parser.add_argument('--base-port', type=int, default=7001,
                        help="Base mediaserver port, default is 7001")
    parser.add_argument('--thread-count', type=int, default=POST_TRANSACTION_GENERATOR_THREAD_COUNT,
                        help=("Transaction generator thread count,"
                              + " default is %d." % POST_TRANSACTION_GENERATOR_THREAD_COUNT))
    parser.add_argument('rate', type=float, help="Transaction rate, per second")
    parser.add_argument('server_count', type=int, help="Mediaserver count")
    parser.add_argument('server_address', nargs='+', help="Mediaserver address")
    
    args = parser.parse_args()

    init_logging(args.logging_config)
    if not args.verbose:
        post_stamp_logger.setLevel(logging.WARNING)

    server_list = address_and_count_to_server_list(
        args.server_address, args.server_count, args.base_port, args.lws)

    metrics_collector = MetricsCollector()
    transaction_gen = transaction_generator(server_list)
    exception_event = threading.Event()
    stop_event = threading.Event()

    with transactions_posted(
            metrics_collector, transaction_gen, args.rate, args.thread_count,
            stop_event, exception_event):
        while not exception_event.wait(10):
            metrics_collector.report(args.rate, duration=timedelta(seconds=10))
        stop_event.set()  # stop all threads at once, not one at a time


if __name__ == '__main__':
    _generation_main()
