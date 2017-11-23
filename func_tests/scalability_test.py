'''Scalability test

  Measure system synchronization time
'''

import datetime
import json
import logging
import traceback
from functools import wraps
from multiprocessing import Pool as ThreadPool

import pytest
from requests.exceptions import ReadTimeout

import resource_synchronization_test as resource_test
import server_api_data_generators as generator
import test_utils.utils as utils
import transaction_log
from memory_usage_metrics import load_host_memory_usage
from test_utils.compare import compare_values
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import GrowingSleep

log = logging.getLogger(__name__)


SET_RESOURCE_STATUS_CMD = '202'
CHECK_METHOD_TIMEOUT = datetime.timedelta(minutes=2)
CHECK_METHOD_RETRY_COUNT = 5


@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=2,
        USE_LIGHTWEIGHT_SERVERS=False,
        CAMERAS_PER_SERVER=1,
        STORAGES_PER_SERVER=1,
        USERS_PER_SERVER=1,
        PROPERTIES_PER_CAMERA=5,
        MERGE_TIMEOUT=MEDIASERVER_MERGE_TIMEOUT,
        REST_API_TIMEOUT=datetime.timedelta(minutes=1),
        )

@pytest.fixture
def lightweight_servers(metrics_saver, lightweight_servers_factory, config):
    assert config.SERVER_COUNT > 1, repr(config.SERVER_COUNT)  # Must be at least 2 servers
    if not config.USE_LIGHTWEIGHT_SERVERS:
        return []
    lws_count = config.SERVER_COUNT - 1
    log.info('Creating %d lightweight servers:', lws_count)
    start_time = utils.datetime_utc_now()
    # at least one full/real server is required for testing
    lws_list = lightweight_servers_factory(
        lws_count,
        CAMERAS_PER_SERVER=config.CAMERAS_PER_SERVER,
        USERS_PER_SERVER=config.USERS_PER_SERVER,
        PROPERTIES_PER_CAMERA=config.PROPERTIES_PER_CAMERA,
        )
    log.info('Created %d lightweight servers', len(lws_list))
    metrics_saver.save('lws_server_init_duration', utils.datetime_utc_now() - start_time)
    assert lws_list, 'No lightweight servers were created'
    return lws_list

@pytest.fixture
def servers(metrics_saver, server_factory, lightweight_servers, config):
    server_count = config.SERVER_COUNT - len(lightweight_servers)
    log.info('Creating %d servers:', server_count)
    setup_settings = dict(systemSettings=dict(
        autoDiscoveryEnabled=utils.bool_to_str(False),
        synchronizeTimeWithInternet=utils.bool_to_str(False),
        ))
    start_time = utils.datetime_utc_now()
    server_list = [server_factory('server_%04d' % (idx + 1),
                           setup_settings=setup_settings,
                           rest_api_timeout=config.REST_API_TIMEOUT)
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
        server.rest_api.get_api_fn('POST', 'ec2', api_method)(json=resource_data)
        req_duration += utils.datetime_utc_now() - req_start_time
        resources.append((server, resource_data))
    duration = utils.datetime_utc_now() - start_time
    requests = len(sequence)
    log.info('%r ec2/%s: total requests=%d, total duration=%s (%s), avg request duration=%s (%s)' % (
        server, api_method, requests, duration, req_duration, duration / requests, req_duration / requests))
    return resources


def get_server_admin(server):
    admins = [(server, u) for u in server.rest_api.ec2.getUsers.GET() if u['isAdmin']]
    assert admins
    return admins[0]


def with_traceback(fn):
    @wraps(fn)  # critical for Pool.map to work
    def wrapper(*args, **kw):
        try:
            return fn(*args, **kw)
        except:
            for line in traceback.format_exc().splitlines():
                log.error(line)
            raise
    return wrapper

@with_traceback
def create_test_data_on_server((config, server, index)):
    resource_generators = dict(
        saveCamera=resource_test.SeedResourceWithParentGenerator(generator.generate_camera_data, index * config.CAMERAS_PER_SERVER),
        saveCameraUserAttributes=resource_test.ResourceGenerator(generator.generate_camera_user_attributes_data),
        setResourceParams=resource_test.SeedResourceList(generator.generate_resource_params_data_list, config.PROPERTIES_PER_CAMERA),
        saveUser=resource_test.SeedResourceGenerator(generator.generate_user_data, index * config.USERS_PER_SERVER),
        saveStorage=resource_test.SeedResourceWithParentGenerator(generator.generate_storage_data, index * config.STORAGES_PER_SERVER),
        saveLayout=resource_test.LayoutGenerator(index * (config.USERS_PER_SERVER + 1)))
    servers_with_guids = [(server, server.ecs_guid)]
    users = create_resources_on_server_by_size(
        server, 'saveUser',  resource_generators, config.USERS_PER_SERVER)
    users.append(get_server_admin(server))
    cameras = create_resources_on_server(server, 'saveCamera',
                                         resource_generators, servers_with_guids * config.CAMERAS_PER_SERVER)
    create_resources_on_server(server, 'saveStorage',
                               resource_generators, servers_with_guids * config.STORAGES_PER_SERVER)
    layout_items_generator = resource_generators['saveLayout'].items_generator
    layout_items_generator.set_resources(cameras)
    create_resources_on_server(server, 'saveLayout', resource_generators, users)
    create_resources_on_server(server, 'saveCameraUserAttributes', resource_generators, cameras)
    create_resources_on_server(server, 'setResourceParams', resource_generators, cameras)


def create_test_data(config, servers):
    server_tupples = [(config, server, i)
                      for i, server in enumerate(servers)]
    pool = ThreadPool(len(servers))
    pool.map(create_test_data_on_server, server_tupples)
    pool.close()
    pool.join()


# merge  ============================================================================================

def get_response(server, method, api_object, api_method):
    for i in range(CHECK_METHOD_RETRY_COUNT):
        try:
            return server.rest_api.get_api_fn(method, api_object, api_method)(timeout=CHECK_METHOD_TIMEOUT)
        except ReadTimeout as x:
            log.error('ReadTimeout when waiting for %s call %s/%s: %s', server, api_object, api_method, x)
        except Exception as x:
            log.error("%s call '%s/%s' error: %s", server, api_object, api_method, x)
    log.error('Retry count exceeded limit (%d) for %s call %s/%s; seems server is deadlocked, will make core dump.',
              CHECK_METHOD_RETRY_COUNT, server, api_object, api_method)
    server.make_core_dump()
    raise  # reraise last exception

def clean_transaction_log(json):
    # We have to filter 'setResourceStatus' transactions due to VMS-5969
    def is_not_set_resource_status(transaction):
        return transaction.command != SET_RESOURCE_STATUS_CMD

    return filter(is_not_set_resource_status, transaction_log.transactions_from_json(json))

def clean_full_info(json):
    # We have to not check 'resStatusList' section due to VMS-5969
    return {k: v for k, v in json.iteritems() if k !='resStatusList'}

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
            log.debug(line)
    else:
        log.warning('Strange, no diffs are found...')

def save_json_artifact(artifact_factory, api_method, side_name, server, value):
    file_path = artifact_factory(['result', api_method, side_name], name='%s-%s' % (api_method, side_name),
                                 ext='.json', type_name='json', content_type='application/json').produce_file_path()
    with open(file_path, 'w') as f:
        json.dump(value, f, indent=4, cls=transaction_log.TransactionJsonEncoder)
    log.debug('results from %s from server %s %s is stored to %s', api_method, server.title, server, file_path)


def make_dumps_and_fail(message, servers, merge_timeout, api_method, api_call_start_time):
    full_message = 'Servers did not merge in %s: %s; currently waiting for method %r for %s' % (
        merge_timeout, message, api_method, utils.datetime_utc_now() - api_call_start_time)
    log.info(full_message)
    log.info('killing servers for core dumps')
    for server in servers:
        server.make_core_dump()
    pytest.fail(full_message)

def wait_for_method_matched(artifact_factory, servers, method, api_object, api_method, start_time, merge_timeout):
    growing_delay = GrowingSleep()
    api_call_start_time = utils.datetime_utc_now()
    while True:
        expected_result_dirty = get_response(servers[0], method, api_object, api_method)
        if expected_result_dirty is None:
            if utils.datetime_utc_now() - start_time >= merge_timeout:
                message = 'server %r has not responded' % server[0]
                make_dumps_and_fail(message, servers, merge_timeout, api_method, api_call_start_time)
            continue
        expected_result = clean_json(api_method, expected_result_dirty)

        def check(servers):
            for srv in servers:
                result = get_response(srv, method, api_object, api_method)
                result_cleaned = clean_json(api_method, result)
                if result_cleaned != expected_result:
                    return (srv, result_cleaned)
            return (None, None)
    
        first_unsynced_server, unmatched_result = check(servers[1:])
        if not first_unsynced_server:
            log.info('%s/%s merge duration: %s' % (api_object, api_method,
                                                   utils.datetime_utc_now() - api_call_start_time))
            return
        if utils.datetime_utc_now() - start_time >= merge_timeout:
            log.info('Servers %s and %s still has unmatched results for method %r:', servers[0], first_unsynced_server, api_method)
            log_diffs(expected_result, unmatched_result)
            save_json_artifact(artifact_factory, api_method, 'x', servers[0], expected_result)
            save_json_artifact(artifact_factory, api_method, 'y', first_unsynced_server, unmatched_result)
            message = 'Servers %s and %s returned different responses' % (servers[0], first_unsynced_server)
            make_dumps_and_fail(message, servers, merge_timeout, api_method, api_call_start_time)
        growing_delay.sleep()


def wait_for_data_merged(artifact_factory, servers, merge_timeout, start_time):
    for api_method in [
            'getUsers',
            'getStorages',
            'getLayouts',
            'getCamerasEx',
            'getFullInfo',
            'getTransactionLog',
            ]:
        wait_for_method_matched(artifact_factory, servers, 'GET', 'ec2', api_method, start_time, merge_timeout)


def collect_additional_metrics(metrics_saver, servers, lightweight_servers):
    if lightweight_servers:
        reply = lightweight_servers[0].rest_api.api.p2pStats.GET()
        metrics_saver.save('total_bytes_sent', int(reply['totalBytesSent']))
        # for test with lightweight servers pick only hosts with lightweight servers
        host_set = set(server.host for server in lightweight_servers)
    else:
        host_set = set(server.host for server in servers)
    for host in host_set:
        metrics = load_host_memory_usage(host)
        for name in 'total used free used_swap mediaserver lws'.split():
            metric_name = 'host_memory_usage.%s.%s' % (host.name, name)
            metric_value = getattr(metrics, name)
            metrics_saver.save(metric_name, metric_value)


def test_scalability(artifact_factory, metrics_saver, config, lightweight_servers, servers):
    assert isinstance(config.MERGE_TIMEOUT, datetime.timedelta)

    # lightweight servers create data themselves
    if not lightweight_servers:
        create_test_data(config, servers)

    start_time = utils.datetime_utc_now()

    if lightweight_servers:
        lightweight_servers[0].wait_until_synced(config.MERGE_TIMEOUT)
        for server in servers:
            server.merge_systems(lightweight_servers[0], take_remote_settings=True)
    else:
        servers[0].merge(servers[1:])

    try:
        wait_for_data_merged(artifact_factory, lightweight_servers + servers, config.MERGE_TIMEOUT, start_time)
        merge_duration = utils.datetime_utc_now() - start_time
        metrics_saver.save('merge_duration', merge_duration)
        collect_additional_metrics(metrics_saver, servers, lightweight_servers)
    finally:
        if servers[0].is_started():
            servers[0].load_system_settings(log_settings=True)  # log final settings
    assert utils.str_to_bool(servers[0].settings['autoDiscoveryEnabled']) == False
