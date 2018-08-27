"""Scalability test

Measure system synchronization time.
"""

import datetime
import logging
from collections import namedtuple
from contextlib import contextmanager
from functools import wraps
from multiprocessing.dummy import Pool as ThreadPool

import pytest
from netaddr import IPNetwork

import framework.utils as utils
import resource_synchronization_test as resource_test
import server_api_data_generators as generator
import transaction_log
from framework.compare import compare_values
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.mediaserver_api import MediaserverApiRequestError
from framework.merging import merge_systems
from framework.switched_logging import SwitchedLogger, with_logger
from framework.utils import GrowingSleep
from memory_usage_metrics import load_host_memory_usage

pytest_plugins = ['fixtures.unpacked_mediaservers']

_logger = SwitchedLogger(__name__)


SET_RESOURCE_STATUS_CMD = '202'


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


# resource creation  ================================================================================

_create_test_data_logger = _logger.getChild('create_test_data')


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
    _create_test_data_logger.info('%s ec2/%s: total requests=%d, total duration=%s (%s), avg request duration=%s (%s)' % (
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
        except Exception:
            _logger.exception('Exception in %r:', fn)
            raise
    return wrapper


@with_traceback
@with_logger(_create_test_data_logger, 'framework.http_api')
@with_logger(_create_test_data_logger, 'framework.mediaserver_api')
def create_test_data_on_server(server_tuple):
    config, server, index = server_tuple
    _logger.info('Create test data on server %s:', server)
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
        server, 'saveUser', resource_generators, config.USERS_PER_SERVER)
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
    _logger.info('Create test data on server %s: complete.', server)


def create_test_data(config, servers):
    server_tuples = [(config, server, i) for i, server in enumerate(servers)]
    pool = ThreadPool(len(servers))
    pool.map(create_test_data_on_server, server_tuples)
    pool.close()
    pool.join()


# merge  ============================================================================================

def get_response(server, api_method):
    try:
        # TODO: Find out whether retries are needed.
        # Formerly, request was retried 5 times regardless of error type.
        # Retry will be reintroduced if server is forgiven for 4 failures.
        return server.api.generic.get(api_method, timeout=120)
    except MediaserverApiRequestError:
        _logger.error("{} may have been deadlocked.", server)
        status = server.service.status()
        if status.is_running:
            server.os_access.make_core_dump(status.pid)
        raise


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
        if not server:
            continue  # env.lws is None for full servers
        status = server.service.status()
        if status.is_running:
            server.os_access.make_core_dump(status.pid)
    pytest.fail(full_message)


def wait_for_method_matched(artifact_factory, merge_timeout, env, api_method):
    _logger.info('Wait for method %r results are merged:', api_method)
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
            _logger.info(
                'Wait for method %r results are merged: done, merge duration: %s',
                api_method, utils.datetime_utc_now() - api_call_start_time)
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


_merge_logger = _logger.getChild('merge')

@with_logger(_merge_logger, 'framework.waiting')
@with_logger(_merge_logger, 'framework.http_api')
@with_logger(_merge_logger, 'framework.mediaserver_api')
def wait_for_data_merged(artifact_factory, merge_timeout, env):
    _logger.info('Wait for all data are merged:')
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
    _logger.info('Wait for all data are merged: done.')


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


_post_check_logger = _logger.getChild('post_check')

@with_logger(_post_check_logger, 'framework.http_api')
@with_logger(_post_check_logger, 'framework.mediaserver_api')
def perform_post_checks(env):
    _logger.info('Perform test post checks:')
    if env.real_server_list[0].api.is_online():
        settings = env.real_server_list[0].api.get_system_settings()  # log final settings
        assert utils.str_to_bool(settings['autoDiscoveryEnabled']) is False
    _logger.info('Perform test post checks: done.')


def test_scalability(artifact_factory, metrics_saver, config, env):
    assert isinstance(config.MERGE_TIMEOUT, datetime.timedelta)

    try:
        wait_for_data_merged(artifact_factory, config.MERGE_TIMEOUT, env)
        merge_duration = utils.datetime_utc_now() - env.merge_start_time
        metrics_saver.save('merge_duration', merge_duration)
        collect_additional_metrics(metrics_saver, env.os_access_set, env.lws)
    finally:
        perform_post_checks(env)

    for server in env.real_server_list:
        assert not server.installation.list_core_dumps()
    if env.lws:
        assert not env.lws.installation.list_core_dumps()
