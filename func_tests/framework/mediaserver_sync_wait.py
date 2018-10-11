'''Wait for mediaservers to synchronize between themselves'''

import logging

from .context_logger import context_logger
from .data_differ import log_diff_list, full_info_differ, transaction_log_differ
from .installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from .message_bus import message_bus_running
from .utils import datetime_utc_now
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

    def log_and_dump_results(self, artifact_factory):
        _logger.info('Servers %s and %s still has unmatched %r:',
                     self.first_server, self.mismatched_server, self.api_path)
        log_diff_list(_logger.info, self.diff_list)
        self._save_json_artifact(artifact_factory, self.first_server, self.first_result)
        self._save_json_artifact(artifact_factory, self.mismatched_server, self.mismatched_result)

    def _save_json_artifact(self, artifact_factory, server, value):
        method_name = self.api_path.replace('/', '-')
        part_list = ['result', method_name, server.name]
        artifact = artifact_factory(part_list, name='%s-%s' % (method_name, server.name))
        file_path = artifact.save_as_json(value)
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

        for server in server_list[1:]:
            result = api_path_getter(server, api_path)
            diff_list = differ.diff(result_0, result)
            if diff_list:
                break
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
        artifact_factory,
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
        e.log_and_dump_results(artifact_factory)
        raise
