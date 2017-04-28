'''Resource syncronization tests

   It tests that servers of the same system sinchronize its data correctly
'''
import logging
import pytest
import time
import itertools
from multiprocessing.dummy import Pool as ThreadPool
from functools import total_ordering
from test_utils.server import MEDIASERVER_MERGE_TIMEOUT_SEC
import server_api_data_generators as generator


log = logging.getLogger(__name__)


DEFAULT_CONFIG_SECTION = 'General'
DEFAULT_TEST_SIZE = 10
DEFAULT_THREAD_NUMBER = 8
CONFIG_USERNAME = 'username'
CONFIG_PASSWORD = 'password'
CONFIG_TESTSIZE = 'testCaseSize'
CONFIG_THREADS = 'threadNumber'


@total_ordering
class Timestamp(object):

    def __init__(self, sequence, ticks):
        self.sequence = sequence
        self.ticks = ticks

    def __eq__(self, other):
        return ((self.sequence, self.ticks) ==
                (other.sequence, other.ticks))

    def __lt__(self, other):
        return ((self.sequence, self.ticks) <
                (other.sequence, other.ticks))

    def __str__(self):
        return "%s.%s" % (self.sequence, self.ticks)


@total_ordering
class Transaction(object):

    @classmethod
    def from_dict(cls, d):
        return Transaction(
            command=d['tran']['command'],
            author=d['tran']['historyAttributes']['author'],
            peer_id=d['tran']['peerID'],
            db_id=d['tran']['persistentInfo']['dbID'],
            sequence=d['tran']['persistentInfo']['sequence'],
            timestamp=Timestamp(
                sequence=d['tran']['persistentInfo']['timestamp']['sequence'],
                ticks=d['tran']['persistentInfo']['timestamp']['ticks']),
            transaction_type=d['tran']['transactionType'],
            transaction_guid=d['tranGuid'])

    def __init__(self, command, author, peer_id, db_id, sequence, timestamp,
                 transaction_type, transaction_guid):
        self.command = command
        self.author = author
        self.peer_id = peer_id
        self.db_id = db_id
        self.sequence = sequence
        self.timestamp = timestamp
        self.transaction_type = transaction_type
        self.transaction_guid = transaction_guid

    def __hash__(self):
        return hash((self.peer_id, self.db_id, self.sequence))

    def __eq__(self, other):
        return ((self.peer_id, self.db_id, self.sequence) ==
                (other.peer_id, other.db_id, other.sequence))

    def __lt__(self, other):
        return ((self.peer_id, self.db_id, self.sequence) <
                (other.peer_id, other.db_id, other.sequence))

    def __str__(self):
        return "Transaction(from: %s, timestamp: %s, command: %s)" % (self.peer_id, self.timestamp, self.command)


class ResourceGenerator(object):

    def __init__(self, gen_fn):
        self.gen_fn = gen_fn

    def get(self, server, val):
        return self.gen_fn(val)


class SeedResourceGenerator(ResourceGenerator):

    _seed = 0

    def __init__(self, gen_fn):
        ResourceGenerator.__init__(self, gen_fn)

    def next(self):
        self._seed += 1
        return self._seed

    def get(self, server, val):
        return self.gen_fn(self.next())


class SeedResourceWithParentGenerator(SeedResourceGenerator):

    def get(self, server, val):
        return self.gen_fn(self.next(), parentId=generator.get_resource_id(val))


class LayoutItemGenerator(SeedResourceGenerator):

    def __init__(self):
        ResourceGenerator.__init__(self, generator.generate_layout_item)
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

    def __init__(self):
        ResourceGenerator.__init__(self, generator.generate_layout_data)
        self.items_generator = LayoutItemGenerator()

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
        setResourceParams=ResourceGenerator(generator.generate_resource_params_data),
        saveStorage=SeedResourceWithParentGenerator(generator.generate_storage_data),
        saveLayout=LayoutGenerator())


@pytest.fixture(params=['merged', 'unmerged'])
def merge_schema(request):
    return request.param


@pytest.fixture
def env(env_builder, server, merge_schema):
    one = server()
    two = server()
    if merge_schema == 'merged':
        boxes_env = env_builder(merge_servers=[one, two],  one=one, two=two)
    else:
        boxes_env = env_builder(one=one, two=two)
    boxes_env.test_size = DEFAULT_TEST_SIZE
    boxes_env.thread_number = DEFAULT_THREAD_NUMBER
    boxes_env.system_is_merged = merge_schema == 'merged'
    boxes_env.resource_generators = resource_generators()
    return boxes_env


def get_response(server, method, api_object, api_method):
    return server.rest_api.get_api_fn(method, api_object, api_method)()


def wait_entity_merge_done(servers, method, api_object, api_method):
    log.info('TEST for %s %s.%s:', method, api_object, api_method)
    start = time.time()
    servers = servers.values()
    while True:
        result_expected = get_response(servers[0], method, api_object, api_method)

        def check(servers, result_expected):
            for srv in servers:
                result = get_response(srv, method, api_object, api_method)
                if result != result_expected:
                    return (srv, result)
            return None

        result = check(servers[1:], result_expected)
        if not result:
            return
        if time.time() - start >= MEDIASERVER_MERGE_TIMEOUT_SEC:
            log.error("'%s' was not synchronized in %d seconds: '%r' and '%r'" % (
                api_method, MEDIASERVER_MERGE_TIMEOUT_SEC, servers[0].box, result[0].box))
            assert result[1] == result_expected
        time.sleep(MEDIASERVER_MERGE_TIMEOUT_SEC / 10.)


def check_api_calls(env, calls):
    for method, api_object, api_method in calls:
        wait_entity_merge_done(env.servers, method, api_object, api_method)


def check_transaction_log(env):
    log.info('TEST for GET ec2.getTransactionLog:')
    servers = env.servers.values()

    def servers_to_str(servers):
        return ', '.join("%r" % s.box for s in servers)

    def transactions_to_str(transactions):
        return '\n  '.join("%s: [%s]" % (k, servers_to_str(v)) for k, v in transactions.iteritems())

    start = time.time()
    while True:
        srv_transactions = {}
        for srv in servers:
            transactions = map(Transaction.from_dict, srv.rest_api.ec2.getTransactionLog.GET())
            for t in transactions:
                srv_transactions.setdefault(t, []).append(srv)
        unmatched_transactions = {t: l for t, l in srv_transactions.iteritems()
                                  if len(l) != len(servers)}
        if not unmatched_transactions:
            return
        if time.time() - start >= MEDIASERVER_MERGE_TIMEOUT_SEC:
            assert False, "Unmatched transaction:\n  %s" % transactions_to_str(unmatched_transactions)
        time.sleep(MEDIASERVER_MERGE_TIMEOUT_SEC / 10.)


def server_api_post((server, api_method, data)):
    server_api_fn = server.rest_api.get_api_fn('POST', 'ec2', api_method)
    return server_api_fn(json=data)


def merge_system_if_unmerged(env):
    if env.system_is_merged:
        return
    servers = env.servers.values()
    assert len(servers) >= 2
    for i in range(len(servers) - 1):
        servers[i+1].merge_systems(servers[i])
        env.system_is_merged = True


def get_servers_admins(env):
    '''Return list of pairs (mediaserver, user data).

    Get admin users from all tested servers
    '''
    admins = []
    for server in env.servers.values():
        users = server.rest_api.ec2.getUsers.GET()
        admins += [(server, u) for u in users if u['isAdmin']]
    return admins


def get_server_by_index(env, i):
    '''Return Server object.'''
    servers = env.servers.values()
    server_i = i % len(servers)
    return servers[server_i]


def prepare_call_list(env, api_method, sequence=None):
    '''Return list of tupples (mediaserver, REST API function name, data to POST).

    Prepare data for the NX media server POST request:
      * env - test environment
      * api_method - media server REST API function name
      * sequence - list of pairs (server, data for resource generation)
    '''
    data_generator = env.resource_generators[api_method]
    sequence = sequence or [(None, i) for i in range(env.test_size)]
    call_list = []

    # The function is used to select server for modification request.
    # If we have already merged system, we can use any server to create/remove/modify resource,
    # otherwise we have to use only resource owner for modification.
    def get_server_for_modification(env, i, server):
        '''Return server for modification.

        * env - test environment
        * i - index for getting server by index
        * server - server where resource have been created
        '''
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
    '''Return list of pairs (mediaserver, posted data).

    Make async NX media server REST API POST requests.
    '''
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
    for srv in env.servers.values():
        for method, api_object, api_method in test_api_get_methods:
            srv.rest_api.get_api_fn(method, api_object, api_method)()


def test_camera_data_syncronization(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    prepare_and_make_async_post_calls(env, 'saveCameraUserAttributes', cameras)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getCamerasEx'),
         ('GET', 'ec2', 'getCameraUserAttributesList'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_user_data_syncronization(env):
    prepare_and_make_async_post_calls(env, 'saveUser')
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_mediaserver_data_syncronization(env):
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
    env_servers = env.servers.values()
    servers = [env_servers[i % len(env_servers)] for i in range(env.test_size)]
    server_with_guid_list = map(lambda s: (s, s.ecs_guid), servers)
    storages = prepare_and_make_async_post_calls(env, 'saveStorage', server_with_guid_list)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getStorages'),
         ('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)


def test_resource_params_data_syncronization(env):
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


def test_remove_resource_params_data_syncronization(env):
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


def test_layout_data_syncronization(env):
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
    env_servers = env.servers.values()
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
        server_1 = env_servers[i % len(env_servers)]
        server_2 = env_servers[(i+1) % len(env_servers)]
        api_calls.append((server_1, api_method, data))
        api_calls.append((server_2, 'removeResource', dict(id=data['id'])))
    make_async_post_calls(env, api_calls)
    check_api_calls(
        env,
        [('GET', 'ec2', 'getFullInfo')])
    check_transaction_log(env)
