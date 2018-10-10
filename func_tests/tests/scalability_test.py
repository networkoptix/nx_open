"""Scalability test

Measure system synchronization time.
"""

import datetime
import json
from collections import namedtuple
from contextlib import contextmanager
import itertools
from functools import partial

import pytest
from netaddr import IPNetwork

import framework.utils as utils
from framework.data_differ import full_info_differ, transaction_log_differ
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.mediaserver_api import MediaserverApiRequestError
from framework.mediaserver_sync_wait import (
    SyncWaitTimeout,
    wait_for_api_path_match,
    wait_until_no_transactions_from_servers,
    )
from framework.merging import merge_systems
from framework.context_logger import ContextLogger, context_logger
from framework.utils import (
    make_threaded_async_calls,
    MultiFunction,
    single,
    take_some,
    with_traceback,
    )
from system_load_metrics import load_host_memory_usage
import server_api_data_generators as generator

pytest_plugins = ['system_load_metrics', 'fixtures.unpacked_mediaservers']

_logger = ContextLogger(__name__)
_create_test_data_logger = _logger.getChild('create_test_data')
_post_check_logger = _logger.getChild('post_check')
_dumper_logger = _logger.getChild('dumper')


SET_RESOURCE_STATUS_CMD = '202'
CREATE_DATA_THREAD_NUMBER = 16


@pytest.fixture()
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=2,
        USE_LIGHTWEIGHT_SERVERS=False,
        CAMERAS_PER_SERVER=1,
        STORAGES_PER_SERVER=1,
        USERS_PER_SERVER=1,
        PROPERTIES_PER_CAMERA=5,
        MERGE_TIMEOUT=MEDIASERVER_MERGE_TIMEOUT,
        MESSAGE_BUS_TIMEOUT=datetime.timedelta(seconds=10),
        MESSAGE_BUS_SERVER_COUNT=5,
        HOST_LIST=None,  # use vm servers by default
        )


# resource creation  ================================================================================


def get_server_admin(server):
    admins = [(server, u) for u in server.api.generic.get('ec2/getUsers') if u['isAdmin']]
    assert admins
    return admins[0]


make_async_calls = partial(make_threaded_async_calls, CREATE_DATA_THREAD_NUMBER)


MAX_LAYOUT_ITEMS = 10


def offset_range(server_idx, count):
    for idx in range(count):
        yield (server_idx * count) + idx


def offset_enumerate(server_idx, item_list):
    for idx, item in enumerate(item_list):
        yield ((server_idx * len(item_list)) + idx, item)


def make_server_async_calls(config, layout_item_id_gen, server_idx, server):

    server_id = server.api.get_server_id()
    admin_user = single(user for user
                          in server.api.generic.get('ec2/getUsers')
                          if user['isAdmin'])
    camera_list = [
        generator.generate_camera_data(camera_id=idx + 1, parentId=server_id)
        for idx in offset_range(server_idx, config.CAMERAS_PER_SERVER)]


    def layout_item_generator(resource_list):
        for idx in layout_item_id_gen:
            resource = resource_list[idx % len(resource_list)]
            yield generator.generate_layout_item(idx + 1, resource['id'])


    layout_item_gen = layout_item_generator(resource_list=camera_list)


    def make_layout_item_list(layout_idx):
        # layouts have 0, 1, 2, .. MAX_LAYOUT_ITEMS-1 items count
        count = layout_idx % MAX_LAYOUT_ITEMS
        return list(take_some(layout_item_gen, count))


    def camera_call_generator():
        for idx, camera in offset_enumerate(server_idx, camera_list):
            camera_user = generator.generate_camera_user_attributes_data(camera)
            camera_param_list = generator.generate_resource_params_data_list(
                idx + 1, camera, list_size=config.PROPERTIES_PER_CAMERA)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveCameraUserAttributes', camera_user),
                partial(server.api.generic.post, 'ec2/setResourceParams', camera_param_list),
                ])


    def other_call_generator():
        for idx in offset_range(server_idx, config.STORAGES_PER_SERVER):
            storage = generator.generate_storage_data(storage_id=idx + 1, parentId=server_id)
            yield partial(server.api.generic.post, 'ec2/saveStorage', storage)

        # 1 layout per user + 1 for admin
        layout_idx_gen = itertools.count(server_idx * (config.USERS_PER_SERVER + 1))

        for idx in offset_range(server_idx, config.USERS_PER_SERVER):
            user = generator.generate_user_data(user_id=idx + 1)
            layout_idx = next(layout_idx_gen)
            user_layout = generator.generate_layout_data(
                layout_id=layout_idx + 1,
                parentId=user['id'],
                items=make_layout_item_list(layout_idx),
                )
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveUser', user),
                partial(server.api.generic.post, 'ec2/saveLayout', user_layout),
                ])

        layout_idx = next(layout_idx_gen)
        admin_layout = generator.generate_layout_data(
            layout_id=layout_idx + 1,
            parentId=admin_user['id'],
            items=make_layout_item_list(layout_idx),
            )
        yield partial(server.api.generic.post, 'ec2/saveLayout', admin_layout)


    return (camera_call_generator(), other_call_generator())


@context_logger(_create_test_data_logger, 'framework.http_api')
@context_logger(_create_test_data_logger, 'framework.mediaserver_api')
def create_test_data(config, server_list):
    _logger.info('Create test data for %d servers:', len(server_list))
    layout_item_id_gen = itertools.count()  # global for all layout items

    # layout calls depend on all cameras being created, so must be called strictly after them
    camera_call_list = []
    other_call_list = []
    for server_idx, server in enumerate(server_list):
        camera_calls, other_calls = make_server_async_calls(
            config, layout_item_id_gen, server_idx, server)
        camera_call_list += list(camera_calls)
        other_call_list += list(other_calls)

    make_async_calls(camera_call_list)
    make_async_calls(other_call_list)
    _logger.info('Create test data: done.')


# merge  ============================================================================================

def make_core_dumps(env):
    _logger.info("Producing servers core dumps.")
    for server in [env.lws] + env.real_server_list:
        if not server:
            continue  # env.lws is None for full servers
        server.make_core_dump_if_running()


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


def wait_for_servers_synced(artifact_factory, config, env):
    wait_until_no_transactions_from_servers(
        env.real_server_list[:1], config.MESSAGE_BUS_TIMEOUT.total_seconds())
    real_server_count = len(env.real_server_list)
    if real_server_count > config.MESSAGE_BUS_SERVER_COUNT:
        server_list = [env.real_server_list[i]
                           for i in range(1, real_server_count, real_server_count/config.MESSAGE_BUS_SERVER_COUNT)]
        wait_until_no_transactions_from_servers(server_list, config.MESSAGE_BUS_TIMEOUT.total_seconds())

    def wait_for_match(api_path, differ):
        wait_for_api_path_match(env.all_server_list, api_path, differ,
                                config.MERGE_TIMEOUT.total_seconds(), api_path_getter)

    try:
        wait_for_match('ec2/getFullInfo', full_info_differ)
        wait_for_match('ec2/getTransactionLog', transaction_log_differ)
    except SyncWaitTimeout as e:
        e.log_and_dump_results(artifact_factory)
        make_core_dumps(env)
        raise


# ===================================================================================================

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


system_settings = dict(
    autoDiscoveryEnabled=utils.bool_to_str(False),
    synchronizeTimeWithInternet=utils.bool_to_str(False),
    )

server_config = dict(
    p2pMode=True,
    serverGuid='8e25e200-0000-0000-{group_idx:04d}-{server_idx:012d}',
    )

Env = namedtuple('Env', 'all_server_list real_server_list lws os_access_set merge_start_time')


@contextmanager
def lws_env(config, groups):
    with groups.one_allocated_server('standalone', system_settings, server_config) as server:
        with groups.allocated_lws(
                server_count=config.SERVER_COUNT - 1,  # minus one standalone
                merge_timeout_sec=config.MERGE_TIMEOUT.total_seconds(),
                CAMERAS_PER_SERVER=config.CAMERAS_PER_SERVER,
                STORAGES_PER_SERVER=config.STORAGES_PER_SERVER,
                USERS_PER_SERVER=config.USERS_PER_SERVER,
                PROPERTIES_PER_CAMERA=config.PROPERTIES_PER_CAMERA,
                ) as lws:
            merge_start_time = utils.datetime_utc_now()
            server.api.merge(lws[0].api, lws.server_bind_address, lws[0].port, take_remote_settings=True)
            yield Env(
                all_server_list=[server] + lws.servers,
                real_server_list=[server],
                lws=lws,
                os_access_set={server.os_access, lws.os_access},
                merge_start_time=merge_start_time,
                )


@with_traceback
def make_real_servers_env(config, server_list, common_net):
    # lightweight servers create data themselves
    create_test_data(config, server_list)
    merge_start_time = utils.datetime_utc_now()
    for server in server_list[1:]:
        merge_systems(server_list[0], server, accessible_ip_net=common_net)
    return Env(
        all_server_list=server_list,
        real_server_list=server_list,
        lws=None,
        os_access_set=set(server.os_access for server in server_list),
        merge_start_time=merge_start_time,
        )


@contextmanager
def unpack_env(config, groups):
    with groups.many_allocated_servers(
            config.SERVER_COUNT, system_settings, server_config) as server_list:
        yield make_real_servers_env(config, server_list, IPNetwork('0.0.0.0/0'))


@pytest.fixture(scope='session')
def two_vm_types():
    return 'linux', 'linux'


@pytest.fixture()
def vm_env(two_clean_mediaservers, config):
    for server in two_clean_mediaservers:
        server.api.setup_local_system(system_settings)
    return make_real_servers_env(config, two_clean_mediaservers, IPNetwork('10.254.0.0/16'))


@pytest.fixture()
def env(request, unpacked_mediaserver_factory, config):
    if config.HOST_LIST:
        groups = unpacked_mediaserver_factory.from_host_config_list(config.HOST_LIST)
        if config.USE_LIGHTWEIGHT_SERVERS:
            with lws_env(config, groups) as env:
                yield env
        else:
            with unpack_env(config, groups) as env:
                yield env
    else:
        yield request.getfixturevalue('vm_env')


@context_logger(_post_check_logger, 'framework.http_api')
@context_logger(_post_check_logger, 'framework.mediaserver_api')
def perform_post_checks(env):
    _logger.info('Perform test post checks:')
    if env.real_server_list[0].api.is_online():
        settings = env.real_server_list[0].api.get_system_settings()  # log final settings
        assert utils.str_to_bool(settings['autoDiscoveryEnabled']) is False
    _logger.info('Perform test post checks: done.')


def dump_full_info(artifacts_dir, env):
    server = env.real_server_list[0]
    _logger.info('Making final dump for first server %s:', server)
    with context_logger(_dumper_logger, 'framework.mediaserver_api'):
        full_info = server.api.generic.get('ec2/getFullInfo')
    with artifacts_dir.joinpath('full-info.json').open('wb') as f:
        json.dump(full_info, f, indent=2)

def test_scalability(artifact_factory, artifacts_dir, metrics_saver, load_averge_collector, config, env):
    assert isinstance(config.MERGE_TIMEOUT, datetime.timedelta)

    try:
        with load_averge_collector(env.os_access_set):
            wait_for_servers_synced(artifact_factory, config, env)
        merge_duration = utils.datetime_utc_now() - env.merge_start_time
        metrics_saver.save('merge_duration', merge_duration)
        collect_additional_metrics(metrics_saver, env.os_access_set, env.lws)
        dump_full_info(artifacts_dir, env)
    finally:
        perform_post_checks(env)

    for server in env.real_server_list:
        assert not server.installation.list_core_dumps()
    if env.lws:
        assert not env.lws.installation.list_core_dumps()
