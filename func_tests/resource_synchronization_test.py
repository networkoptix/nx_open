import itertools
import logging
import time
from multiprocessing.dummy import Pool as ThreadPool

import pytest

import server_api_data_generators as generator
import transaction_log
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT
from test_utils.utils import SimpleNamespace, datetime_utc_now

log = logging.getLogger(__name__)


DEFAULT_TEST_SIZE = 10
DEFAULT_THREAD_NUMBER = 8


class ResourceGenerator(object):

    def __init__(self, gen_fn):
        self.gen_fn = gen_fn

    def get(self, server, val):
        return self.gen_fn(val)


class SeedResourceGenerator(ResourceGenerator):

    def __init__(self, gen_fn, initial=0):
        ResourceGenerator.__init__(self, gen_fn)
        self._seed = initial

    def next(self):
        self._seed += 1
        return self._seed

    def get(self, server, val):
        return self.gen_fn(self.next())


class SeedResourceWithParentGenerator(SeedResourceGenerator):

    def get(self, server, val):
        return self.gen_fn(self.next(), parentId=generator.get_resource_id(val))


class SeedResourceList(SeedResourceGenerator):

    def __init__(self, gen_fn, list_size, initial=0):
        SeedResourceGenerator.__init__(self,  gen_fn, initial)
        self._list_size = list_size

    def get(self, server, val):
        return self.gen_fn(self.next(), val, self._list_size)


class LayoutItemGenerator(SeedResourceGenerator):

    def __init__(self, initial=0):
        SeedResourceGenerator.__init__(self, generator.generate_layout_item, initial)
        self.__resources = dict()

    def set_resources(self, resources):
        for server, resource in resources:
            self.__resources.setdefault(server.ecs_guid, []).append(
                generator.get_resource_id(resource))

    def get_resources_by_server(self, server):
        if server:
            return self.__resources[server.ecs_guid]
        else:
            return list(
                itertools.chain.from_iterable(self.__resources.values()))

    def get(self, server, val):
        resources = self.get_resources_by_server(server)
        assert resources
        return [self.gen_fn(self.next(), resources[i % len(resources)])
                for i in range(val)]


class LayoutGenerator(SeedResourceGenerator):

    MAX_LAYOUT_ITEMS = 10

    def __init__(self, initial=0):
        SeedResourceGenerator.__init__(self, generator.generate_layout_data, initial)
        self.items_generator = LayoutItemGenerator(initial * self.MAX_LAYOUT_ITEMS)

    def get(self, server, val):
        items = self.items_generator.get(server, self._seed % self.MAX_LAYOUT_ITEMS)
        return self.gen_fn(self.next(),
                           parentId=generator.get_resource_id(val),
                           items=items)


def resource_generators():
    return dict(
        saveCamera=SeedResourceGenerator(generator.generate_camera_data),
        saveUser=SeedResourceGenerator(generator.generate_user_data),
        saveMediaServer=SeedResourceGenerator(generator.generate_mediaserver_data),
        saveCameraUserAttributes=ResourceGenerator(generator.generate_camera_user_attributes_data),
        saveMediaServerUserAttributes=ResourceGenerator(generator.generate_mediaserver_user_attributes_data),
        removeResource=ResourceGenerator(generator.generate_remove_resource_data),
        setResourceParams=SeedResourceList(generator.generate_resource_params_data_list, 1),
        saveStorage=SeedResourceWithParentGenerator(generator.generate_storage_data),
        saveLayout=LayoutGenerator())


@pytest.fixture(params=['merged', 'unmerged'])
def merge_schema(request):
    return request.param


@pytest.fixture
def env(server_factory, merge_schema):
    one = server_factory('one')
    two = server_factory('two')
    if merge_schema == 'merged':
        one.merge([two])
    return SimpleNamespace(
        one=one,
        two=two,
        servers=[one, two],
        test_size=DEFAULT_TEST_SIZE,
        thread_number=DEFAULT_THREAD_NUMBER,
        system_is_merged=merge_schema == 'merged',
        resource_generators=resource_generators(),
        )


def get_response(server, method, api_object, api_method):
    return server.rest_api.get_api_fn(method, api_object, api_method)()


def wait_entity_merge_done(servers, method, api_object, api_method):
    log.info('TEST for %s %s.%s:', method, api_object, api_method)
    start_time = datetime_utc_now()
    while True:
        result_expected = get_response(servers[0], method, api_object, api_method)

        def check(servers, result_expected):
            for srv in servers:
                result = get_response(srv, method, api_object, api_method)
                if result != result_expected:
                    return srv, result
            return None

        result = check(servers[1:], result_expected)
        if not result:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            log.error("'%s' was not synchronized in %s: '%r' and '%r'" % (
                api_method, MEDIASERVER_MERGE_TIMEOUT, servers[0], result[0]))
            assert result[1] == result_expected
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.)


def check_api_calls(env, calls):
    for method, api_object, api_method in calls:
        wait_entity_merge_done(env.servers, method, api_object, api_method)


def check_transaction_log(env):
    log.info('TEST for GET ec2.getTransactionLog:')

    def servers_to_str(servers):
        return ', '.join("%r" % s for s in servers)

    def transactions_to_str(transactions):
        return '\n  '.join("%s: [%s]" % (k, servers_to_str(v)) for k, v in transactions.items())

    start_time = datetime_utc_now()
    while True:
        srv_transactions = {}
        for srv in env.servers:
            transactions = transaction_log.transactions_from_json(
                srv.rest_api.ec2.getTransactionLog.GET())
            for t in transactions:
                srv_transactions.setdefault(t, []).append(srv)
        unmatched_transactions = {t: l for t, l in srv_transactions.items()
                                  if len(l) != len(env.servers)}
        if not unmatched_transactions:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert False, "Unmatched transaction:\n  %s" % transactions_to_str(unmatched_transactions)
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.)


def server_api_post((server, api_method, data)):
    server_api_fn = server.rest_api.get_api_fn('POST', 'ec2', api_method)
    return server_api_fn(json=data)


def merge_system_if_unmerged(env):
    if env.system_is_merged:
        return
    assert len(env.servers) >= 2
    for i in range(len(env.servers) - 1):
        env.servers[i+1].merge_systems(env.servers[i])
        env.system_is_merged = True


def get_servers_admins(env):
    """Return list of pairs (mediaserver, user data).

    Get admin users from all tested servers
    """
    admins = []
    for server in env.servers:
        users = server.rest_api.ec2.getUsers.GET()
        admins += [(server, u) for u in users if u['isAdmin']]
    return admins


def get_server_by_index(env, i):
    """Return Server object."""
    server_i = i % len(env.servers)
    return env.servers[server_i]


def prepare_call_list(env, api_method, sequence=None):
    """Return list of tupples (mediaserver, REST API function name, data to POST).

    Prepare data for the NX media server POST request:
      * env - test environment
      * api_method - media server REST API function name
      * sequence - list of pairs (server, data for resource generation)
    """
    data_generator = env.resource_generators[api_method]
    sequence = sequence or [(None, i) for i in range(env.test_size)]
    call_list = []

    # The function is used to select server for modification request.
    # If we have already merged system, we can use any server to create/remove/modify resource,
    # otherwise we have to use only resource owner for modification.
    def get_server_for_modification(env, i, server):
        """Return server for modification.

        * env - test environment
        * i - index for getting server by index
        * server - server where resource have been created
        """
        if not server or env.system_is_merged:
            # If the resource's server isn't specified or system is already merged,
            # get server for modification request by index
            return get_server_by_index(env, i)
        else:
            # Otherwise, get the resource's owner for modification.
            return server

    for i, v in enumerate(sequence[:env.test_size]):
        server, val = v
        # Get server for the modification request.
        server_for_modification = get_server_for_modification(env, i, server)
        # Get server for resource data generation,
        # data generator object should use only the server's resources for unmerged system.
        # 'None' means that any server can be used for generation.
        resource_owner = None if env.system_is_merged else server
        resource_data = data_generator.get(resource_owner, val)
        call_list.append((server_for_modification, api_method, resource_data))
    return call_list


def make_async_post_calls(env, call_list):
    """Return list of pairs (mediaserver, posted data).

    Make async NX media server REST API POST requests.
    """
    pool = ThreadPool(env.thread_number)
    pool.map(server_api_post, call_list)
    pool.close()
    pool.join()
    return map(lambda d: (d[0], d[2]), call_list)


def prepare_and_make_async_post_calls(env, api_method, sequence=None):
    return make_async_post_calls(env,
                                 prepare_call_list(env, api_method, sequence))


@pytest.mark.parametrize('merge_schema', ['merged'])
def test_initial_merge(env):
    check_api_calls(
        env,
        [('GET', 'ec2', 'getResourceParams'),
         ('GET', 'ec2', 'getMediaServersEx'),
         ('GET', 'ec2', 'getCamerasEx'),
         ('GET', 'ec2', 'getUsers'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


@pytest.mark.parametrize('merge_schema', ['merged'])
def test_api_get_methods(env):
    test_api_get_methods = [
        ('GET', 'ec2', 'getResourceTypes'),
        ('GET', 'ec2', 'getResourceParams'),
        ('GET', 'ec2', 'getMediaServers'),
        ('GET', 'ec2', 'getMediaServersEx'),
        ('GET', 'ec2', 'getCameras'),
        ('GET', 'ec2', 'getCamerasEx'),
        ('GET', 'ec2', 'getCameraHistoryItems'),
        ('GET', 'ec2', 'bookmarks/tags'),
        ('GET', 'ec2', 'getEventRules'),
        ('GET', 'ec2', 'getUsers'),
        ('GET', 'ec2', 'getVideowalls'),
        ('GET', 'ec2', 'getLayouts'),
        ('GET', 'ec2', 'listDirectory'),
        ('GET', 'ec2', 'getStoredFile'),
        ('GET', 'ec2', 'getSettings'),
        ('GET', 'ec2', 'getCurrentTime'),
        ('GET', 'ec2', 'getFullInfo'),
        ('GET', 'ec2', 'getLicenses')
        ]
    for srv in env.servers:
        for method, api_object, api_method in test_api_get_methods:
            srv.rest_api.get_api_fn(method, api_object, api_method)()


def test_camera_data_synchronization(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    prepare_and_make_async_post_calls(env, 'saveCameraUserAttributes', cameras)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getCamerasEx'),
         ('GET', 'ec2', 'getCameraUserAttributesList'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_user_data_synchronization(env):
    prepare_and_make_async_post_calls(env, 'saveUser')
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_mediaserver_data_synchronization(env):
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    prepare_and_make_async_post_calls(env, 'saveMediaServerUserAttributes', servers)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getMediaServersEx'),
         ('GET', 'ec2', 'getMediaServersUserAttributesList'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_storage_data_synchronization(env):
    servers = [env.servers[i % len(env.servers)] for i in range(env.test_size)]
    server_with_guid_list = map(lambda s: (s, s.ecs_guid), servers)
    storages = prepare_and_make_async_post_calls(env, 'saveStorage', server_with_guid_list)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getStorages'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_resource_params_data_synchronization(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    resources = [v[i % 3] for i, v in enumerate(zip(cameras, users, servers))]
    prepare_and_make_async_post_calls(env, 'setResourceParams', resources)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getResourceParams'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_remove_resource_params_data_synchronization(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    storages = prepare_and_make_async_post_calls(env, 'saveStorage', servers)
    # Need to remove different resource types (camera, user, mediaserver)
    resources = [v[i % 4] for i, v in enumerate(zip(cameras, users, servers, storages))]
    prepare_and_make_async_post_calls(env, 'removeResource', resources)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_layout_data_synchronization(env):
    admins = get_servers_admins(env)
    admins_seq = [admins[i % len(admins)] for i in range(env.test_size)]
    # A shared layout doesn't have a parent
    shared_layouts_seq = [(get_server_by_index(env, i), 0) for i in range(env.test_size)]
    users = prepare_and_make_async_post_calls(env, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    layout_items_generator = env.resource_generators['saveLayout'].items_generator
    layout_items_generator.set_resources(cameras + servers)
    user_layouts = prepare_and_make_async_post_calls(env, 'saveLayout', users)
    admin_layouts = prepare_and_make_async_post_calls(env, 'saveLayout', admins_seq)
    shared_layouts = prepare_and_make_async_post_calls(env, 'saveLayout', shared_layouts_seq)
    layouts_to_remove_count = env.test_size / 2
    prepare_and_make_async_post_calls(env, 'removeResource', user_layouts[:layouts_to_remove_count])
    prepare_and_make_async_post_calls(env, 'removeResource', admin_layouts[:layouts_to_remove_count])
    prepare_and_make_async_post_calls(env, 'removeResource', shared_layouts[:layouts_to_remove_count])
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getLayouts'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


@pytest.mark.parametrize('merge_schema', ['merged'])
def test_resource_remove_update_conflict(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    storages = prepare_and_make_async_post_calls(env, 'saveStorage', servers)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getCamerasEx'),
         ('GET', 'ec2', 'getUsers'),
         ('GET', 'ec2', 'getStorages'),
         ('GET', 'ec2', 'getMediaServersEx')])
    api_methods = ['saveCamera', 'saveUser', 'saveMediaServer', 'saveStorage']
    # Prepare api calls list, the list contains save.../removeResource pairs.
    # Each pair has the same resource to reach a conflict.
    api_calls = []
    # Need to check different resource types (camera, user, mediaserver).
    for i, v in enumerate(zip(cameras, users, servers, storages)):
        api_method = api_methods[i % 4]
        data = v[i % 4][1]
        data['name'] += '_changed'
        server_1 = env.servers[i % len(env.servers)]
        server_2 = env.servers[(i+1) % len(env.servers)]
        api_calls.append((server_1, api_method, data))
        api_calls.append((server_2, 'removeResource', dict(id=data['id'])))
    make_async_post_calls(env, api_calls)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)
