"""Scalability test

Measure system synchronization time.
"""

from datetime import timedelta
import json
import threading

import pytest
from netaddr import IPNetwork

from framework.context_logger import ContextLogger, context_logger
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.mediaserver_sync_wait import SyncWaitTimeout
from framework.utils import datetime_local_now, str_to_bool
from setup_servers import (
    SetupConfig,
    system_settings,
    make_real_servers_env,
    servers_set_up,
    )
from resource_synchronization import wait_for_servers_synced
from metrics import MetricsCollector
from transaction_stage import transaction_stage_running
from system_load_metrics import load_host_memory_usage


pytest_plugins = ['system_load_metrics', 'fixtures.unpacked_mediaservers']


_logger = ContextLogger('scalability')
_post_check_logger = _logger.getChild('post_check')
_dumper_logger = _logger.getChild('dumper')


TRANSACTION_GENERATOR_THREAD_NUMBER = 16


@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=2,
        USE_LIGHTWEIGHT_SERVERS=False,
        CAMERAS_PER_SERVER=1,
        STORAGES_PER_SERVER=1,
        USERS_PER_SERVER=1,
        PROPERTIES_PER_CAMERA=5,
        TRANSACTIONS_PER_SERVER_RATE=10.0,
        MERGE_TIMEOUT=MEDIASERVER_MERGE_TIMEOUT,
        MESSAGE_BUS_TIMEOUT=timedelta(seconds=3),
        MESSAGE_BUS_SERVER_COUNT=5,
        TRANSACTIONS_STAGE_DURATION=timedelta(seconds=10),
        HOST_LIST=None,  # use vm servers by default
        )


@pytest.fixture
def setup_config(config):
    return SetupConfig.from_pytest_config(config)


@pytest.fixture(scope='session')
def two_vm_types():
    return 'linux', 'linux'


@pytest.fixture
def vm_env(two_clean_mediaservers, setup_config):
    for server in two_clean_mediaservers:
        server.api.setup_local_system(system_settings)
    return make_real_servers_env(setup_config, list(two_clean_mediaservers), IPNetwork('10.254.0.0/16'))


@pytest.fixture
def env(request, unpacked_mediaserver_factory, setup_config):
    if setup_config.host_list:
        with servers_set_up(setup_config, unpacked_mediaserver_factory) as env:
            yield env
    else:
        yield request.getfixturevalue('vm_env')


def pick_some_items(seq, count):
    """Return items allocated distributed evenly thru sequence"""
    count = min(len(seq), count)
    return [seq[len(seq) * i // count] for i in range(count)]


def save_transaction_stage_metrics(metrics_saver, metrics_collector, stage_duration):
    metrics = metrics_collector.get_and_reset()
    actual_rate = metrics.posted_transaction_count / stage_duration.total_seconds()
    metrics_saver.save('post_transaction_duration.average', metrics.post_transaction_duration.average)
    metrics_saver.save('post_transaction_duration.max', metrics.post_transaction_duration.max)
    metrics_saver.save('transaction_stale.average', metrics.stale_time.average)
    metrics_saver.save('transaction_stale.max', metrics.stale_time.max)
    metrics_saver.save('transaction_delay.average', metrics.transaction_delay.average)
    metrics_saver.save('transaction_delay.max', metrics.transaction_delay.max)
    metrics_saver.save('websocket_reopened_count', metrics_collector.socket_reopened_counter.get())


def run_transaction_stage(metrics_saver, load_averge_collector, config, env):
    if not config.TRANSACTIONS_STAGE_DURATION:
        _logger.info("Transaction stage is skipped.")
        return
    transaction_rate = config.TRANSACTIONS_PER_SERVER_RATE * len(env.all_server_list)
    watch_server_list = pick_some_items(env.all_server_list, config.MESSAGE_BUS_SERVER_COUNT)
    metrics_collector = MetricsCollector()
    exception_event = threading.Event()
    stop_event = threading.Event()

    with transaction_stage_running(
            metrics_collector,
            env.all_server_list,
            transaction_rate,
            TRANSACTION_GENERATOR_THREAD_NUMBER,
            watch_server_list,
            stop_event,
            exception_event,
            ):
        with load_averge_collector(env.os_access_set, 'transactions'):
            exception_event.wait(config.TRANSACTIONS_STAGE_DURATION.total_seconds())
        stop_event.set()  # stop all generating threads at once, not one at a time
    save_transaction_stage_metrics(metrics_saver, metrics_collector, config.TRANSACTIONS_STAGE_DURATION)


def make_core_dumps(env):
    _logger.info("Producing servers core dumps.")
    for server in [env.lws] + env.real_server_list:
        if not server:
            continue  # env.lws is None for full servers
        server.make_core_dump_if_running()


def collect_additional_metrics(metrics_saver, os_access_set, lws):
    if lws:
        reply = lws[0].api.generic.get('api/p2pStats')
        metrics_saver.save('total_bytes_sent', int(reply['totalBytesSent']))
        # for test with lightweight servers pick only hosts with lightweight servers
        os_access_set = {lws.os_access}
    for os_access in os_access_set:
        metrics = load_host_memory_usage(os_access)
        for name in 'total used free used_swap mediaserver lws'.split():
            metric_name = 'host_memory_usage.%s.%s' % (os_access.alias, name)
            metric_value = getattr(metrics, name)
            metrics_saver.save(metric_name, metric_value)


def dump_full_info(artifacts_dir, env):
    server = env.real_server_list[0]
    _logger.info('Making final dump for first server %s:', server)
    with context_logger(_dumper_logger, 'framework.mediaserver_api'):
        full_info = server.api.generic.get('ec2/getFullInfo')
    with artifacts_dir.joinpath('full-info.json').open('wb') as f:
        json.dump(full_info, f, indent=2)


@context_logger(_post_check_logger, 'framework.http_api')
@context_logger(_post_check_logger, 'framework.mediaserver_api')
def perform_post_checks(env):
    _logger.info('Perform test post checks:')
    if env.real_server_list[0].api.is_online():
        settings = env.real_server_list[0].api.get_system_settings()  # log final settings
        assert str_to_bool(settings['autoDiscoveryEnabled']) is False
    _logger.info('Perform test post checks: done.')


def test_scalability(artifact_factory, artifacts_dir, metrics_saver, load_averge_collector, config, env):
    try:
        with load_averge_collector(env.os_access_set, 'merge'):
            try:
                wait_for_servers_synced(
                    env,
                    config.MERGE_TIMEOUT,
                    config.MESSAGE_BUS_TIMEOUT,
                    config.MESSAGE_BUS_SERVER_COUNT,
                    )
            except SyncWaitTimeout as e:
                e.log_and_dump_results(artifact_factory)
                make_core_dumps(env)
                raise
        metrics_saver.save('merge_duration', datetime_local_now() - env.merge_start_time)

        run_transaction_stage(metrics_saver, load_averge_collector, config, env)

        collect_additional_metrics(metrics_saver, env.os_access_set, env.lws)
        dump_full_info(artifacts_dir, env)
    finally:
        perform_post_checks(env)

    for server in env.real_server_list:
        assert not server.installation.list_core_dumps()
    if env.lws:
        assert not env.lws.installation.list_core_dumps()
