"""Scalability test

Measure system synchronization time.
"""

import datetime
import logging
import traceback
from collections import namedtuple
from contextlib import contextmanager
from functools import wraps
from multiprocessing.dummy import Pool as ThreadPool

import pytest
from netaddr.ip import IPNetwork
from requests.exceptions import ReadTimeout

import framework.utils as utils
import resource_synchronization_test as resource_test
import server_api_data_generators as generator
import transaction_log
from framework.compare import compare_values
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.merging import (
    find_accessible_mediaserver_address,
    find_any_mediaserver_address,
    merge_systems,
    )
from framework.networking import setup_flat_network
from framework.utils import GrowingSleep
from memory_usage_metrics import load_host_memory_usage

pytest_plugins = ['fixtures.unpacked_mediaservers']

_logger = logging.getLogger(__name__)


SET_RESOURCE_STATUS_CMD = '202'
CHECK_METHOD_RETRY_COUNT = 5


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
        HOST_LIST=None,  # use vm servers by default
        )


@pytest.fixture()
def lightweight_servers(metrics_saver, lightweight_servers_factory, config):
    assert config.SERVER_COUNT > 1, repr(config.SERVER_COUNT)  # Must be at least 2 servers
    if not config.USE_LIGHTWEIGHT_SERVERS:
        return []
    lws_count = config.SERVER_COUNT - 1
    _logger.info('Creating %d lightweight servers:', lws_count)
    start_time = utils.datetime_utc_now()
    # at least one full/real server is required for testing
    lws_list = lightweight_servers_factory(
        lws_count,
        CAMERAS_PER_SERVER=config.CAMERAS_PER_SERVER,
        USERS_PER_SERVER=config.USERS_PER_SERVER,
        PROPERTIES_PER_CAMERA=config.PROPERTIES_PER_CAMERA,
        )
    _logger.info('Created %d lightweight servers', len(lws_list))
    metrics_saver.save('lws_server_init_duration', utils.datetime_utc_now() - start_time)
    assert lws_list, 'No lightweight servers were created'
    return lws_list


@pytest.fixture()
def servers(metrics_saver, linux_mediaservers_pool, lightweight_servers, config):
    server_count = config.SERVER_COUNT - len(lightweight_servers)
    _logger.info('Creating %d servers:', server_count)
    setup_settings = dict(systemSettings=dict(
        autoDiscoveryEnabled=utils.bool_to_str(False),
        synchronizeTimeWithInternet=utils.bool_to_str(False),
        ))
    start_time = utils.datetime_utc_now()
    server_list = [
        linux_mediaservers_pool.get(
            'server_%04d' % (idx + 1),
            setup_settings=setup_settings)
        for idx in range(server_count)]
    metrics_saver.save('server_init_duration', utils.datetime_utc_now() - start_time)
    return server_list


# resource creation  ================================================================================

def create_resources_on_server_by_size(server, api_method, resource_generators, size):
    sequence = [(server, i) for i in range(size)]
    return create_resources_on_server(server, api_method, resource_generators, sequence)


def create_resources_on_server(server, api_method, resource_generators, sequence):
    data_generator = resource_generators[api_method]
    sequence = sequence
    resources = []
    start_time = utils.datetime_utc_now()
    req_duration = datetime.timedelta(0)
    for v in sequence:
        _, val = v
        resource_data = data_generator.get(server, val)
        req_start_time = utils.datetime_utc_now()
        server.api.generic.post('ec2/' + api_method, resource_data)
        req_duration += utils.datetime_utc_now() - req_start_time
        resources.append((server, resource_data))
    duration = utils.datetime_utc_now() - start_time
    requests = len(sequence)
    _logger.info('%r ec2/%s: total requests=%d, total duration=%s (%s), avg request duration=%s (%s)' % (
        server, api_method, requests, duration, req_duration, duration / requests, req_duration / requests))
    return resources


def get_server_admin(server):
    admins = [(server, u) for u in server.api.generic.get('ec2/getUsers') if u['isAdmin']]
    assert admins
    return admins[0]


def with_traceback(fn):
    @wraps(fn)  # critical for VMFactory.map to work
    def wrapper(*args, **kw):
        try:
            return fn(*args, **kw)
        except:
            for line in traceback.format_exc().splitlines():
                _logger.error(line)
            raise
    return wrapper


@with_traceback
def create_test_data_on_server((config, server, index)):
    resource_generators = dict(
        saveCamera=resource_test.SeedResourceWithParentGenerator(
            generator.generate_camera_data,
            index * config.CAMERAS_PER_SERVER),
        saveCameraUserAttributes=resource_test.ResourceGenerator(
            generator.generate_camera_user_attributes_data),
        setResourceParams=resource_test.SeedResourceList(
            generator.generate_resource_params_data_list,
            config.PROPERTIES_PER_CAMERA),
        saveUser=resource_test.SeedResourceGenerator(
            generator.generate_user_data,
            index * config.USERS_PER_SERVER),
        saveStorage=resource_test.SeedResourceWithParentGenerator(
            generator.generate_storage_data,
            index * config.STORAGES_PER_SERVER),
        saveLayout=resource_test.LayoutGenerator(
            index * (config.USERS_PER_SERVER + 1)))
    servers_with_guids = [(server, server.api.get_server_id())]
    users = create_resources_on_server_by_size(
        server, 'saveUser',  resource_generators, config.USERS_PER_SERVER)
    users.append(get_server_admin(server))
    cameras = create_resources_on_server(
        server, 'saveCamera', resource_generators, servers_with_guids * config.CAMERAS_PER_SERVER)
    create_resources_on_server(
        server, 'saveStorage', resource_generators, servers_with_guids * config.STORAGES_PER_SERVER)
    layout_items_generator = resource_generators['saveLayout'].items_generator
    layout_items_generator.set_resources(cameras)
    create_resources_on_server(server, 'saveLayout', resource_generators, users)
    create_resources_on_server(server, 'saveCameraUserAttributes', resource_generators, cameras)
    create_resources_on_server(server, 'setResourceParams', resource_generators, cameras)


def create_test_data(config, servers):
    server_tuples = [(config, server, i) for i, server in enumerate(servers)]
    pool = ThreadPool(len(servers))
    pool.map(create_test_data_on_server, server_tuples)
    pool.close()
    pool.join()


# merge  ============================================================================================

def get_response(server, api_method):
    for i in range(CHECK_METHOD_RETRY_COUNT):
        try:
            return server.api.generic.get(api_method, timeout=120)
        except ReadTimeout as x:
            _logger.error('ReadTimeout when waiting for %s call %s: %s', server, api_method, x)
        except Exception as x:
            _logger.error("%s call '%s' error: %s", server, api_method, x)
    _logger.error('Retry count exceeded limit (%d) for %s call %s/%s; seems server is deadlocked, will make core dump.',
              CHECK_METHOD_RETRY_COUNT, server, api_method)
    server.service.make_core_dump()
    raise  # reraise last exception


def clean_transaction_log(json):
    # We have to filter 'setResourceStatus' transactions due to VMS-5969
    def is_not_set_resource_status(transaction):
        return transaction.command != SET_RESOURCE_STATUS_CMD

    return filter(is_not_set_resource_status, transaction_log.transactions_from_json(json))


def clean_full_info(json):
    # We have to not check 'resStatusList' section due to VMS-5969
    return {k: v for k, v in json.items() if k != 'resStatusList'}


def clean_json(api_method, json):
    cleaners = dict(
        getFullInfo=clean_full_info,
        getTransactionLog=clean_transaction_log,
        )
    cleaner = cleaners.get(api_method)
    if json and cleaner:
        return cleaner(json)
    else:
        return json


def log_diffs(x, y):
    lines = compare_values(x, y)
    if lines:
        for line in lines:
            _logger.debug(line)
    else:
        _logger.warning('Strange, no diffs are found...')


def save_json_artifact(artifact_factory, api_method, side_name, server, value):
    part_list = ['result', api_method.replace('/', '-'), side_name]
    artifact = artifact_factory(part_list, name='%s-%s' % (api_method, side_name))
    file_path = artifact.save_as_json(value, encoder=transaction_log.TransactionJsonEncoder)
    _logger.debug('results from %s from server %s %s is stored to %s', api_method, server.name, server, file_path)


def make_dumps_and_fail(env, merge_timeout, api_method, api_call_start_time, message):
    full_message = 'Servers did not merge in %s: %s; currently waiting for method %r for %s' % (
        merge_timeout, message, api_method, utils.datetime_utc_now() - api_call_start_time)
    _logger.info(full_message)
    _logger.info("Producing servers core dumps.")
    for server in [env.lws] + env.real_server_list:
        status = server.service.status()
        if status.is_running:
            server.os_access.make_core_dump(status.pid)
    pytest.fail(full_message)


def wait_for_method_matched(artifact_factory, merge_timeout, env, api_method):
    growing_delay = GrowingSleep()
    api_call_start_time = utils.datetime_utc_now()
    while True:
        server0 = env.all_server_list[0]
        expected_result_dirty = get_response(server0, api_method)
        if expected_result_dirty is None:
            if utils.datetime_utc_now() - env.merge_start_time >= merge_timeout:
                message = 'server %r has not responded' % server0
                make_dumps_and_fail(env, merge_timeout, api_method, api_call_start_time, message)
            continue
        expected_result = clean_json(api_method, expected_result_dirty)

        def check(server_list):
            for server in server_list:
                result = get_response(server, api_method)
                result_cleaned = clean_json(api_method, result)
                if result_cleaned != expected_result:
                    return server, result_cleaned
            return None, None
    
        first_unsynced_server, unmatched_result = check(env.all_server_list[1:])
        if not first_unsynced_server:
            _logger.info('%s merge duration: %s' % (api_method,
                                                    utils.datetime_utc_now() - api_call_start_time))
            return
        if utils.datetime_utc_now() - env.merge_start_time >= merge_timeout:
            _logger.info(
                'Servers %s and %s still has unmatched results for method %r:',
                server0, first_unsynced_server, api_method)
            log_diffs(expected_result, unmatched_result)
            save_json_artifact(artifact_factory, api_method, 'x', server0, expected_result)
            save_json_artifact(artifact_factory, api_method, 'y', first_unsynced_server, unmatched_result)
            message = 'Servers %s and %s returned different responses' % (server0, first_unsynced_server)
            make_dumps_and_fail(env, merge_timeout, api_method, api_call_start_time, message)
        growing_delay.sleep()


def wait_for_data_merged(artifact_factory, merge_timeout, env):
    api_methods_to_check = [
        # 'getMediaServersEx',  # The only method to debug/check if servers are actually able to merge.
        'ec2/getUsers',
        'ec2/getStorages',
        'ec2/getLayouts',
        'ec2/getCamerasEx',
        'ec2/getFullInfo',
        'ec2/getTransactionLog',
        ]
    for api_method in api_methods_to_check:
        wait_for_method_matched(artifact_factory, merge_timeout, env, api_method)


def collect_additional_metrics(metrics_saver, os_access_set, lws):
    if lws:
        reply = lws[0].api.generic.get('api/p2pStats')
        metrics_saver.save('total_bytes_sent', int(reply['totalBytesSent']))
        # for test with lightweight servers pick only hosts with lightweight servers
        os_access_set = set([lws.os_access])
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
    )


Env = namedtuple('Env', 'all_server_list real_server_list lws os_access_set merge_start_time')


@contextmanager
def lws_env(config, groups):
    with groups.allocated_one_server('standalone', system_settings, server_config) as server:
        with groups.allocated_lws(
                server_count=config.SERVER_COUNT - 1,  # minus one standalone
                merge_timeout_sec=config.MERGE_TIMEOUT.total_seconds(),
                CAMERAS_PER_SERVER=config.CAMERAS_PER_SERVER,
                STORAGES_PER_SERVER=config.STORAGES_PER_SERVER,
                USERS_PER_SERVER=config.USERS_PER_SERVER,
                PROPERTIES_PER_CAMERA=config.PROPERTIES_PER_CAMERA,
                ) as lws:
            merge_start_time = utils.datetime_utc_now()
            merge_systems(server, lws[0], take_remote_settings=True, remote_address=lws.address)
            yield Env(
                all_server_list=[server] + lws.servers,
                real_server_list=[server],
                lws=lws,
                os_access_set=set([server.os_access, lws.os_access]),
                merge_start_time=merge_start_time,
                )


def make_real_servers_env(config, server_list, remote_address_picker):
    # lightweight servers create data themselves
    create_test_data(config, server_list)
    merge_start_time = utils.datetime_utc_now()
    for server in server_list[1:]:
        remote_address = remote_address_picker(server)
        merge_systems(server_list[0], server, remote_address=remote_address)
    return Env(
        all_server_list=server_list,
        real_server_list=server_list,
        lws=None,
        os_access_set=set(server.os_access for server in server_list),
        merge_start_time=merge_start_time,
        )


@contextmanager
def unpack_env(config, groups):
    with groups.allocated_many_servers(
            config.SERVER_COUNT, system_settings, server_config) as server_list:
        yield make_real_servers_env(config, server_list, remote_address_picker=find_any_mediaserver_address)


@pytest.fixture
def vm_env(hypervisor, vm_factory, mediaserver_factory, config):
    with vm_factory.allocated_vm('vm-1', vm_type='linux') as vm1:
         with vm_factory.allocated_vm('vm-2', vm_type='linux') as vm2:
            setup_flat_network([vm1, vm2], IPNetwork('10.254.254.0/28'), hypervisor)
            with mediaserver_factory.allocated_mediaserver('server-1', vm1) as server1:
                with mediaserver_factory.allocated_mediaserver('server-2', vm2) as server2:
                    server_list = [server1, server2]
                    for server in server_list:
                        server.start()
                        server.api.setup_local_system(system_settings)
                    yield make_real_servers_env(
                        config, server_list, remote_address_picker=find_accessible_mediaserver_address)


@pytest.fixture
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
    

def test_scalability(artifact_factory, metrics_saver, config, env):
    assert isinstance(config.MERGE_TIMEOUT, datetime.timedelta)

    try:
        wait_for_data_merged(artifact_factory, config.MERGE_TIMEOUT, env)
        merge_duration = utils.datetime_utc_now() - env.merge_start_time
        metrics_saver.save('merge_duration', merge_duration)
        collect_additional_metrics(metrics_saver, env.os_access_set, env.lws)
    finally:
        if env.real_server_list[0].api.is_online():
            settings = env.real_server_list[0].api.get_system_settings()  # log final settings
        assert utils.str_to_bool(settings['autoDiscoveryEnabled']) is False

    for server in env.real_server_list:
        assert not server.installation.list_core_dumps()
    if env.lws:
        assert env.lws.installation.list_core_dumps()
##    lightweight_servers_factory.perform_post_checks()
