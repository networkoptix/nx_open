#!/usr/bin/env python2

# Read transactions from server using message bus; calculate transaction propagation delay.

import argparse
from contextlib import contextmanager
from functools import partial
import logging
import threading

import dateutil.parser

from framework.message_bus import SetResourceParamCommand, message_bus_running
from framework.threaded import ThreadedCall
from local_mediaservers import create_local_server_list
from metrics import MetricsCollector
from cmdline_logging import init_logging


reader_logger = logging.getLogger('scalability.transaction_reader')


def _parse_stamp(stamp):
    server_name, camera_idx, iso_datetime = stamp.split('.', 2)
    return dateutil.parser.parse(iso_datetime)


def _get_transaction(metrics_collector, bus):
    transaction = bus.get_transaction(timeout_sec=0.2)
    if not transaction:
        return
    if not isinstance(transaction.command, SetResourceParamCommand):
        return
    if not transaction.command.param.name == 'scalability-stamp':
        return
    stamp_dt = _parse_stamp(transaction.command.param.value)
    delay = transaction.received_at - stamp_dt
    reader_logger.debug('Transaction from %s propagation delay: %s', transaction.mediaserver_alias, delay)
    metrics_collector.add_transaction_delay(delay)


# post transactions and measure their propagation time
@contextmanager
def transactions_watched(metrics_collector, server_list, exception_event, wait=False):
    if server_list:
        with message_bus_running(server_list, metrics_collector.socket_reopened_counter) as bus:
            if wait:
                reader_logger.info('Waiting until there are no more transactions:')
                bus.wait_until_no_transactions(timeout_sec=10)
                reader_logger.info('Ready.')
            with ThreadedCall.periodic(
                    partial(_get_transaction, metrics_collector, bus),
                    description='reader',
                    exception_event=exception_event,
                    ):
                yield
    else:
        reader_logger.info("No servers to watch for transactions are specified - not watching.")
        yield  # do nothing for empty server list


def _reader_main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logging-config', default='default.yaml',
                        help=("Configuration file for logging, in yaml format."
                              " Relative to logging-config dir if relative."))
    parser.add_argument('--verbose', '-v', action='store_true', help="Verbose logging.")
    parser.add_argument('--lws', action='store_true',
                        help="Use lightweight servers instead of full ones.")
    parser.add_argument('--wait', '-w', action='store_true',
                        help="Wait until no incoming transactions first.")
    parser.add_argument('server_url', nargs='*', help='Mediaserver url (host:port)')
    
    args = parser.parse_args()

    init_logging(args.logging_config)
    if args.logging_config == 'default.yaml':
        logging.getLogger('framework.message_bus').setLevel(logging.WARNING)
    if not args.verbose:
        reader_logger.setLevel(logging.INFO)

    server_list = create_local_server_list(
        args.server_url, password='admin' if args.lws else None)
    metrics_collector = MetricsCollector()

    exception_event = threading.Event()
    with transactions_watched(metrics_collector, server_list, exception_event, wait=args.wait):
        while not exception_event.wait(10):
            metrics_collector.report()


if __name__ == '__main__':
    _reader_main()
