'''Wait for mediaservers to synchronize between themselves'''

from functools import partial
import logging

from .context_logger import context_logger
from .data_differ import log_diff_list, full_info_differ, transaction_log_differ
from .installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from .message_bus import message_bus_running
from .utils import datetime_utc_now, make_threaded_async_calls
from .waiting import WaitTimeout, Wait

_logger = logging.getLogger(__name__)
_sync_logger = _logger.getChild('sync')


DEFAULT_TIMEOUT_SEC = MEDIASERVER_MERGE_TIMEOUT.total_seconds()
DEFAULT_MAX_DELAY_SEC = 60
DEFAULT_BUS_TIMEOUT_SEC = 3


class SyncWaitTimeout(WaitTimeout):

    def __init__(
            self,
            timeout_sec,
            api_path,
            first_server,
            first_result,
            mismatched_server,
            mismatched_result,
            diff_list,
            message,
            ):
        super(SyncWaitTimeout, self).__init__(timeout_sec, message)
        self.api_path = api_path
        self.first_server = first_server
        self.first_result = first_result
        self.mismatched_server = mismatched_server
        self.mismatched_result = mismatched_result
        self.diff_list = diff_list

    def log_and_dump_results(self, artifacts_dir):
        _logger.info('Servers %s and %s still has unmatched %r:',
                     self.first_server, self.mismatched_server, self.api_path)
        log_diff_list(_logger.info, self.diff_list)
        self._save_json_artifact(artifacts_dir, self.first_server, self.first_result)
        self._save_json_artifact(artifacts_dir, self.mismatched_server, self.mismatched_result)

    def _save_json_artifact(self, artifacts_dir, server, value):
        file_name = 'result-{}-{}'.format(self.api_path.replace('/', '-'), server.name)
        file_path = artifacts_dir / file_name
        file_path.write_json(value)
        _logger.debug('Results from %s from server %s %s are stored to %s',
                      self.api_path, server.name, server, file_path)


def _api_path_getter(server, api_path):
    return server.api.generic.get(api_path)


@context_logger(_sync_logger, 'framework.waiting')
@context_logger(_sync_logger, 'framework.http_api')
@context_logger(_sync_logger, 'framework.mediaserver_api')
def wait_for_api_path_match(
        server_list,
        api_path,
        differ,
        timeout_sec=DEFAULT_TIMEOUT_SEC,
        api_path_getter=_api_path_getter,
        max_delay_sec=DEFAULT_MAX_DELAY_SEC,
        ):
    description = '{} servers {!r} results to be same'.format(len(server_list), api_path)
    _logger.info('Wait for %s:', description)
    start_time = datetime_utc_now()
    wait = Wait(description, timeout_sec, max_delay_sec, logger=_logger.getChild('wait'))

    while True:
        server_0 = server_list[0]
        result_0 = api_path_getter(server_0, api_path)

        other_server_list = server_list[1:]

        if len(other_server_list) <= 3:
            result_iter = ((server, api_path_getter(server, api_path))
                           for server in other_server_list)
        else:

            def get_server_and_call_result(server):
                return (server, api_path_getter(server, api_path))

            # Make first 2 calls and checks synchronously,
            # others asynchronously, if they are still required.
            # This is because most of the time first several results are already different,
            # so there is no need to call all servers for them.
            def result_gen():
                for server in other_server_list[:2]:
                    yield (server, api_path_getter(server, api_path))
                for (server, result) in make_threaded_async_calls(thread_count=64, call_gen=[
                        partial(get_server_and_call_result, server) for server in other_server_list[2:]]):
                    yield (server, result)

            result_iter = result_gen()

        for server, result in result_iter:
            diff_list = differ.diff(result_0, result)
            if diff_list:
                break  # server and result from this scope will be used below
        else:
            _logger.info('Wait for %s: done, sync duration: %s',
                         description, datetime_utc_now() - start_time)
            return

        if not wait.again():
            raise SyncWaitTimeout(
                timeout_sec,
                api_path,
                server_0,
                result_0,
                server,
                result,
                diff_list,
                "Timed out ({} seconds) waiting for: {}".format(timeout_sec, description),
                )
        wait.sleep()


def wait_until_no_transactions_from_servers(server_list, timeout_sec):
    _logger.info('Wait for message bus for %s:', server_list)
    with message_bus_running(server_list) as bus:
        bus.wait_until_no_transactions(timeout_sec)
    _logger.info('No more transactions from %s:', server_list)


def wait_for_servers_synced(
        artifacts_dir,
        server_list,
        timeout_sec=DEFAULT_TIMEOUT_SEC,
        bus_timeout_sec=DEFAULT_BUS_TIMEOUT_SEC,
        api_path_getter=_api_path_getter,
        ):
    wait_until_no_transactions_from_servers(server_list[:1], bus_timeout_sec)
    try:
        wait_for_api_path_match(
            server_list, 'ec2/getFullInfo', full_info_differ, timeout_sec, api_path_getter)
        wait_for_api_path_match(
            server_list, 'ec2/getTransactionLog', transaction_log_differ, timeout_sec, api_path_getter)
    except SyncWaitTimeout as e:
        e.log_and_dump_results(artifacts_dir)
        raise
