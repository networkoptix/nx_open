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


log = logging.getLogger(__name__)

MERGE_TIMEOUT = datetime.timedelta(hours=1)
MERGE_DONE_CHECK_PERIOD_SEC = 2.

# Perhaps, it'd better to put it in the test configuration
TOTAL_CAMERA_COUNT = 10000
TOTAL_STORAGE_COUNT = 500
TOTAL_USER_COUNT = 1000
RESOURCES_PER_CAMERA = 5

REST_API_TIMEOUT_SEC = 60.


# Virtual box environment
#
# @pytest.fixture
# def env(env_builder, server, request):
#     servers_count = request.config.getoption('--scalability-servers-count')
#     assert servers_count and servers_count >= 2, (
#         'Invalid servers count %s, more than 2 servers are required for the test' %  servers_count)
#     servers = {'Server_%d' % i: server() for i in range(servers_count) }
#     return env_builder(**servers)


@pytest.fixture
def env(request, test_session, run_options):
    config_path = request.config.getoption('--scalability-yaml-config-file')
    with open(config_path, 'r') as f:
        server_list = yaml.load(f)
    servers = dict()
    for idx, endpoint in enumerate(server_list):
        server_name = 'Server_%d' % idx
        rest_api_url = 'http://%s:%d/' % (endpoint['host'], endpoint['port'])
        server = Server('networkoptix', server_name, rest_api_url,
                        internal_ip_port=endpoint['port'],
                        rest_api_timeout_sec=REST_API_TIMEOUT_SEC)
        server._is_started = True
        server.setup_local_system()
        servers[server_name] = server
    return SimpleNamespace(servers=servers)


def get_response(server, method, api_object, api_method):
    try:
        return server.rest_api.get_api_fn(method, api_object, api_method)()
    except Exception, x:
        log.error("%r call '%s/%s' error: %s" % (server, api_object, api_method, str(x)))
        return None


def wait_merge_done(servers, method, api_object, api_method, start_time):
    api_call_start_time = utils.datetime_utc_now()
    while True:
        result_expected = get_response(servers[0], method, api_object, api_method)

        if not result_expected:
            if utils.datetime_utc_now() - start_time >= MERGE_TIMEOUT:
                pytest.fail("%r can't get response for '%s' during %s" % (
                    servers[0], api_method,
                    MERGE_TIMEOUT  - (api_call_start_time - start_time)))
            continue

        def check(servers, result_expected):
            for srv in servers:
                result = get_response(srv, method, api_object, api_method)
                if result != result_expected:
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


def create_test_data_on_server((server, index, cameras_per_server, users_per_server, storages_per_server)):
    resource_generators = dict(
        saveCamera=resource_test.SeedResourceWithParentGenerator(generator.generate_camera_data, index * cameras_per_server),
        saveCameraUserAttributes=resource_test.ResourceGenerator(generator.generate_camera_user_attributes_data),
        setResourceParams=resource_test.SeedResourceList(generator.generate_resource_params_data_list, RESOURCES_PER_CAMERA),
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


def create_test_data(servers):
    cameras_per_server = TOTAL_CAMERA_COUNT / len(servers)
    users_per_server = TOTAL_USER_COUNT / len(servers)
    storages_per_server = TOTAL_STORAGE_COUNT / len(servers)
    server_tupples = [(server, i, cameras_per_server, users_per_server, storages_per_server)
                      for i, server in enumerate(servers)]
    pool = ThreadPool(len(servers))
    pool.map(create_test_data_on_server, server_tupples)
    pool.close()
    pool.join()


@pytest.mark.skipif(not pytest.config.getoption('--scalability-yaml-config-file'),
                    reason="--scalability-yaml-config-file was not specified")
def test_scalability(env):
    servers = env.servers.values()
    create_test_data(servers)
    measure_merge(servers)
