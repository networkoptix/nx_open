'''Scalability test

  Measure system synchronization time
'''

import pytest
import yaml
import logging
import time
import requests
import datetime
import test_utils.utils as utils
import resource_synchronization_test as resource_test
import server_api_data_generators as generator
from test_utils.utils import SimpleNamespace
from multiprocessing import Pool as ThreadPool
from test_utils.server import Server
import transaction_log

log = logging.getLogger(__name__)


MERGE_TIMEOUT = datetime.timedelta(hours=1)
MERGE_DONE_CHECK_PERIOD_SEC = 2.
SET_RESOURCE_STATUS_CMD = '202'

REST_API_TIMEOUT_SEC = 60.


@pytest.fixture
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=2,
        TOTAL_CAMERA_COUNT=2,
        TOTAL_STORAGE_COUNT=2,
        TOTAL_USER_COUNT=2,
        RESOURCES_PER_CAMERA=2,
        )

@pytest.fixture
def servers(config, server_factory):
    assert config.SERVER_COUNT > 1, repr(config.SERVER_COUNT)  # Must be at least 2 servers
    log.info('Creating %d servers:', config.SERVER_COUNT)
    setup_settings = dict(autoDiscoveryEnabled=False)
    return [server_factory('server_%04d' % (idx + 1), setup_settings=setup_settings)
            for idx in range(config.SERVER_COUNT)]


def get_response(server, method, api_object, api_method):
    try:
        return server.rest_api.get_api_fn(method, api_object, api_method)()
    except Exception, x:
        log.error("%r call '%s/%s' error: %s" % (server, api_object, api_method, str(x)))
        return None


def compare_transaction_log(json_1, json_2):
    # We have to filter 'setResourceStatus' transactions due to VMS-5969
    def is_not_set_resource_status(transaction):
        return transaction.command != SET_RESOURCE_STATUS_CMD

    def clean(json):
        return filter(is_not_set_resource_status, transaction_log.transactions_from_json(json))

    transactions_1 = clean(json_1)
    transactions_2 = clean(json_2)
    return transactions_1 == transactions_2


def compare_full_info(json_1, json_2):
    # We have to not check 'resStatusList' section due to VMS-5969
    def clean(json):
        return {k: v for k, v in json.iteritems() if k != 'resStatusList'}

    full_info_1 = clean(json_1)
    full_info_2 = clean(json_2)
    return full_info_1 == full_info_2


def compare_json_data(api_method, json_1, json_2):
    comparators = {
        'getFullInfo': compare_full_info,
        'getTransactionLog': compare_transaction_log}
    compare_fn = comparators.get(api_method)
    if compare_fn:
        return compare_fn(json_1, json_2)
    return json_1 == json_2


def wait_merge_done(servers, method, api_object, api_method, start_time):
    api_call_start_time = utils.datetime_utc_now()
    while True:
        result_expected = get_response(servers[0], method, api_object, api_method)

        if result_expected is None:
            if utils.datetime_utc_now() - start_time >= MERGE_TIMEOUT:
                pytest.fail("%r can't get response for '%s' during %s" % (
                    servers[0], api_method,
                    MERGE_TIMEOUT - (api_call_start_time - start_time)))
            continue

        def check(servers, result_expected):
            for srv in servers:
                result = get_response(srv, method, api_object, api_method)
                if not compare_json_data(api_method, result, result_expected):
                    return srv
            return None
        first_unsynced_server = check(servers[1:], result_expected)
        if not first_unsynced_server:
            log.info('%s/%s merge duration: %s' % (api_object, api_method,
                                                   utils.datetime_utc_now() - api_call_start_time))
            return
        if utils.datetime_utc_now() - start_time >= MERGE_TIMEOUT:
            pytest.fail("'%s' was not synchronized during %s: '%r' and '%r'" % (
                api_method, MERGE_TIMEOUT - (api_call_start_time - start_time),
                servers[0], first_unsynced_server))
        time.sleep(MERGE_DONE_CHECK_PERIOD_SEC)


def measure_merge(servers):
    start_time = utils.datetime_utc_now()
    for i in range(1, len(servers)):
        servers[0].merge_systems(servers[i])
    wait_merge_done(servers, 'GET', 'ec2', 'getUsers', start_time)
    wait_merge_done(servers, 'GET', 'ec2', 'getStorages', start_time)
    wait_merge_done(servers, 'GET', 'ec2', 'getLayouts', start_time)
    wait_merge_done(servers, 'GET', 'ec2', 'getCamerasEx', start_time)
    wait_merge_done(servers, 'GET', 'ec2', 'getFullInfo', start_time)
    wait_merge_done(servers, 'GET', 'ec2', 'getTransactionLog', start_time)
    log.info('Total merge duration: %s' % (utils.datetime_utc_now() - start_time))


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


def create_test_data_on_server((config, server, index, cameras_per_server, users_per_server, storages_per_server)):
    resource_generators = dict(
        saveCamera=resource_test.SeedResourceWithParentGenerator(generator.generate_camera_data, index * cameras_per_server),
        saveCameraUserAttributes=resource_test.ResourceGenerator(generator.generate_camera_user_attributes_data),
        setResourceParams=resource_test.SeedResourceList(generator.generate_resource_params_data_list, config.RESOURCES_PER_CAMERA),
        saveUser=resource_test.SeedResourceGenerator(generator.generate_user_data, index * users_per_server),
        saveStorage=resource_test.SeedResourceWithParentGenerator(generator.generate_storage_data, index * storages_per_server),
        saveLayout=resource_test.LayoutGenerator(index * (users_per_server + 1)))
    servers_with_guids = [(server, server.ecs_guid)]
    users = create_resources_on_server_by_size(
        server, 'saveUser',  resource_generators, users_per_server)
    users.append(get_server_admin(server))
    cameras = create_resources_on_server(server, 'saveCamera',
                                         resource_generators, servers_with_guids * cameras_per_server)
    create_resources_on_server(server, 'saveStorage',
                               resource_generators, servers_with_guids * storages_per_server)
    layout_items_generator = resource_generators['saveLayout'].items_generator
    layout_items_generator.set_resources(cameras)
    create_resources_on_server(server, 'saveLayout', resource_generators, users)
    create_resources_on_server(server, 'saveCameraUserAttributes', resource_generators, cameras)
    create_resources_on_server(server, 'setResourceParams', resource_generators, cameras)


def create_test_data(config, servers):
    cameras_per_server = config.TOTAL_CAMERA_COUNT / len(servers)
    users_per_server = config.TOTAL_USER_COUNT / len(servers)
    storages_per_server = config.TOTAL_STORAGE_COUNT / len(servers)
    server_tupples = [(config, server, i, cameras_per_server, users_per_server, storages_per_server)
                      for i, server in enumerate(servers)]
    pool = ThreadPool(len(servers))
    pool.map(create_test_data_on_server, server_tupples)
    pool.close()
    pool.join()


def test_scalability(config, servers):
    create_test_data(config, servers)
    measure_merge(servers)
