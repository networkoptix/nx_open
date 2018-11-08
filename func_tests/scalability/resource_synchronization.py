import logging

from framework.data_differ import full_info_differ, transaction_log_differ
from framework.mediaserver_api import MediaserverApiRequestError
from framework.mediaserver_sync_wait import (
    wait_for_api_path_match,
    wait_until_no_transactions_from_servers,
    )


_logger = logging.getLogger(__name__)


def api_path_getter(server, api_path):
    try:
        # TODO: Find out whether retries are needed.
        # Formerly, request was retried 5 times regardless of error type.
        # Retry will be reintroduced if server is forgiven for 4 failures.
        return server.api.generic.get(api_path, timeout=120)
    except MediaserverApiRequestError:
        _logger.error("{} may have been deadlocked. Making core dump.", server)
        server.make_core_dump_if_running()
        raise


def pick_some_items(seq, count):
    """Return items allocated distributed evenly thru sequence"""
    count = min(len(seq), count)
    return [seq[len(seq) * i // count] for i in range(count)]


def wait_for_servers_synced(env, merge_timeout, message_bus_timeout, message_bus_server_count):
    wait_until_no_transactions_from_servers(
        env.real_server_list[:1], message_bus_timeout.total_seconds())
    if len(env.real_server_list) > message_bus_server_count:
        server_list = pick_some_items(env.real_server_list[1:], message_bus_server_count)
        wait_until_no_transactions_from_servers(server_list, message_bus_timeout.total_seconds())

    def wait_for_match(api_path, differ):
        wait_for_api_path_match(env.all_server_list, api_path, differ,
                                merge_timeout.total_seconds(), api_path_getter)

    wait_for_match('ec2/getFullInfo', full_info_differ)
    wait_for_match('ec2/getTransactionLog', transaction_log_differ)
