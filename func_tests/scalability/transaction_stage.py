#!/usr/bin/env python2

# Generate/post test transactions on servers with requested rate.
# Read transactions from server using message bus; calculate transaction propagation delay.

import argparse
from contextlib import contextmanager
from datetime import timedelta
import logging
import threading

from transaction_generator import (
    POST_TRANSACTION_GENERATOR_THREAD_COUNT,
    post_stamp_logger,
    transaction_generator,
    transactions_posted,
    address_and_count_to_server_list,
    )
from transaction_reader import reader_logger, transactions_watched
from metrics import MetricsCollector
from existing_mediaservers import create_existing_server_list
from cmdline_logging import init_logging


@contextmanager
def transaction_stage_running(
        metrics_collector,
        post_server_list,
        post_rate,
        post_thread_count,
        watch_server_list,
        stop_event,
        exception_event,
        ):
    with transactions_watched(metrics_collector, watch_server_list, exception_event, wait=True):
        transaction_gen = transaction_generator(post_server_list)
        with transactions_posted(
                metrics_collector, transaction_gen, post_rate, post_thread_count,
                stop_event, exception_event):
            yield


def _transaction_stage_main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logging-config', default='default.yaml',
                        help=("Configuration file for logging, in yaml format."
                              " Relative to logging-config dir if relative."))
    parser.add_argument('--verbose', '-v', action='store_true', help="Verbose logging.")
    parser.add_argument('--lws', action='store_true',
                        help="Use lightweight servers instead of full ones.")
    parser.add_argument('--base-port', type=int, default=7001,
                        help="Base mediaserver port, default is 7001")
    parser.add_argument('--post-thread-count', type=int, default=POST_TRANSACTION_GENERATOR_THREAD_COUNT,
                        help=("Transaction generator thread count,"
                              + " default is %d." % POST_TRANSACTION_GENERATOR_THREAD_COUNT))
    parser.add_argument('--watch-server-url', nargs='*',
                        help='Mediaserver url (host:port) to watch for transaction at')
    parser.add_argument('post_rate', type=float, help="Transaction post rate, per second")
    parser.add_argument('post_server_count', type=int,
                        help="Mediaservers to post transactions to, total count")
    parser.add_argument('post_server_address', nargs='+',
                        help="Mediaservers to post transactions to, IP address")
    
    args = parser.parse_args()

    init_logging(args.logging_config)
    if args.logging_config == 'default.yaml':
        logging.getLogger('framework.message_bus').setLevel(logging.WARNING)
    if not args.verbose:
        reader_logger.setLevel(logging.INFO)
        post_stamp_logger.setLevel(logging.WARNING)

    watch_server_list = create_existing_server_list(
        args.watch_server_url, password='admin' if args.lws else None)
    post_server_list = address_and_count_to_server_list(
        args.post_server_address, args.post_server_count, args.base_port, args.lws)

    metrics_collector = MetricsCollector()
    exception_event = threading.Event()
    stop_event = threading.Event()

    with transaction_stage_running(
            metrics_collector, post_server_list, args.post_rate, args.post_thread_count,
            watch_server_list, stop_event, exception_event):
        while not exception_event.wait(10):
            metrics_collector.report(args.post_rate, duration=timedelta(seconds=10))
        stop_event.set()  # stop all generating threads at once, not one at a time


if __name__ == '__main__':
    _transaction_stage_main()
