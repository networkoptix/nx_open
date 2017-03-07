'''Resource syncronization tests

   It tests that servers of the same system sinchronize its data correctly
'''
import logging
import pytest
import time
from ConfigParser import ConfigParser
from server import MEDIASERVER_MERGE_TIMEOUT_SEC
from server_rest_api import ServerRestApi
from utils import SimpleNamespace
import server_api_data_generators as generator
from multiprocessing.dummy import Pool as ThreadPool
from functools import total_ordering

log = logging.getLogger(__name__)

DEFAULT_CONFIG_SECTION = 'General'
DEFAULT_TEST_SIZE = 10
DEFAULT_THREAD_NUMBER = 8
CONFIG_USERNAME = 'username'
CONFIG_PASSWORD = 'password'
CONFIG_TESTSIZE = 'testCaseSize'
CONFIG_THREADS = 'threadNumber'
DEFAULT_SLEEP = MEDIASERVER_MERGE_TIMEOUT_SEC / 10.0 # pause between synchronization checks

def read_servers(config):
    server_list = config.get(DEFAULT_CONFIG_SECTION, 'serverList')
    return server_list.split(",")

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
        return Transaction(d['tran']['command'],
                           d['tran']['historyAttributes']['author'],
                           d['tran']['peerID'],
                           d['tran']['persistentInfo']['dbID'],
                           d['tran']['persistentInfo']['sequence'],
                           Timestamp(d['tran']['persistentInfo']['timestamp']['sequence'],
                                     d['tran']['persistentInfo']['timestamp']['ticks']),
                           d['tran']['transactionType'],
                           d['tranGuid'])

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
        return "Transaction#%s timestamp: %s, command: %s" % (self.peer_id, self.timestamp, self.command)

class RemoteServer(object):

    def __init__(self, name, url, user, password):
        self.title = name
        self.url = url
        self.user = user
        self.password = password
        self.ecs_guid = None
        self.settings = None
        self.local_system_id = None
        self.rest_api = ServerRestApi(self.title, self.url, self.user, self.password)
        self.load_system_settings()

    def load_system_settings(self):
        log.debug('%s: Loading settings...', self)
        self.settings = self.get_system_settings()
        self.local_system_id = self.settings['localSystemId']
        self.cloud_system_id = self.settings['cloudSystemID']
        self.ecs_guid = self.rest_api.ec2.testConnection.get()['ecsGuid']

    def get_system_settings(self):
        response = self.rest_api.api.systemSettings.get()
        return response['settings']

class ResourceGenerator(object):
    def __init__(self, gen_fn):
        self.gen_fn = gen_fn

    def get(self, seq):
        for i, v in enumerate(seq):
            yield (i, self.gen_fn(v))

class SeedResourceGenerator(ResourceGenerator):
    __seed = 0

    def __init__(self, gen_fn):
        ResourceGenerator.__init__(self, gen_fn)

    def next(self):
       self.__seed+=1
       return self.__seed

    def get(self, seq):
        for i, v in enumerate(seq):
            yield (i, self.gen_fn(self.next()))

@pytest.fixture(scope="module")
def gen():
    return dict(
        saveCamera=SeedResourceGenerator(generator.generate_camera_data).get,
        saveUser=SeedResourceGenerator(generator.generate_user_data).get,
        saveMediaServer=SeedResourceGenerator(generator.generate_mediaserver_data).get,
        saveCameraUserAttributes=ResourceGenerator(generator.generate_camera_user_attributes_data).get,
        saveMediaServerUserAttributes=ResourceGenerator(generator.generate_mediaserver_user_attributes_data).get,
        removeResource=ResourceGenerator(generator.generate_remove_resource_data).get,
        setResourceParams=ResourceGenerator(generator.generate_resource_params_data).get)

@pytest.fixture
def env(request, env_builder, server):
    servers = []
    config_file = request.config.getoption('--config-file')
    if config_file:
        config = ConfigParser()
        config.read(config_file)
        servers = [RemoteServer('srv-%d' % idx,
                                 'http://%s/' % srv,
                                 config.get(DEFAULT_CONFIG_SECTION, CONFIG_USERNAME),
                                 config.get(DEFAULT_CONFIG_SECTION, CONFIG_PASSWORD))
                   for idx, srv in enumerate(read_servers(config))]
        return SimpleNamespace(servers=servers,
                               test_size=config.get_int(DEFAULT_CONFIG_SECTION, CONFIG_TESTSIZE),
                               thread_number=config.get_int(DEFAULT_CONFIG_SECTION, CONFIG_THREADS))
    else:
        one = server()
        two = server()
        boxes_env = env_builder(merge_servers=[one, two],  one=one, two=two)
        boxes_env.test_size = DEFAULT_TEST_SIZE
        boxes_env.thread_number = DEFAULT_THREAD_NUMBER
        boxes_env.servers = [boxes_env.one, boxes_env.two]
        return boxes_env

def get_response(server, method, api_object, api_method):
    return server.rest_api.get_api_fn(method, api_object, api_method)()

def wait_entity_merge_done(
    servers, method, api_object,
    api_method, timeout = MEDIASERVER_MERGE_TIMEOUT_SEC):
    log.info('TEST for %s %s.%s:', method.upper(), api_object, api_method)
    start = time.time()
    while True:
        result_expected = get_response(servers[0], method, api_object, api_method)
        def check(servers, result_expected):
            for srv in servers:
                result = get_response(srv, method, api_object, api_method)
                if result != result_expected:
                    return (srv, result)
            return None
        result = check(servers[1:], result_expected)
        if not result: return
        if time.time() - start >= timeout:
            log.error("'%s' unsynchronized data: %s(%s) and %s(%s)" % (
                api_method, servers[0].url, servers[0].title, result[0].url, result[0].title))
            assert result[1] == result_expected
        time.sleep(DEFAULT_SLEEP)

def check_api_calls(env, calls):
    for method, api_object, api_method in calls:
        wait_entity_merge_done(env.servers, method, api_object, api_method)

def check_transaction_log(env, timeout = MEDIASERVER_MERGE_TIMEOUT_SEC):
    log.info('TEST for GET ec2.getTransactionLog:')
    def servers_to_str(servers):
        return ', '.join("%s(%s)" % (s.title, s.url) for s in servers)
    def transactions_to_str(transactions):
        return '\n  '.join("%s: [%s]" % (k, servers_to_str(v)) for k, v in transactions.iteritems())
    start = time.time()
    while True:
        srv_transactions = {}
        for srv in env.servers:
            transactions = map(Transaction.from_dict, srv.rest_api.ec2.getTransactionLog.get())
            for t in transactions:
                srv_transactions.setdefault(t, []).append(srv)
        unmatched_transactions = {t: l for t, l in srv_transactions.iteritems()
                                  if len(l) != len(env.servers)}
        if not unmatched_transactions: return
        if time.time() - start >= timeout:
            assert False, "Unmatched transaction:\n  %s" % transactions_to_str(unmatched_transactions)
        time.sleep(DEFAULT_SLEEP)
            

def server_api_post( (server_api_method, data) ):
    if isinstance(data, dict):
        return server_api_method(**data)
    else:
        return server_api_method(json=data)

def prepare_calls_list(env, gen, api_method, sequence = None):
    data_generator_fn = gen[api_method]
    sequence = sequence or range(env.test_size)
    return [(
        env.servers[i % len(env.servers)].rest_api.get_api_fn('post', 'ec2', api_method),
        v) for i, v in data_generator_fn(sequence[:env.test_size])]

def make_async_post_calls(env, calls_list):
    pool = ThreadPool(env.thread_number)
    pool.map(server_api_post, calls_list)
    pool.close()
    pool.join()
    return map(lambda d: d[1], calls_list)

def prepare_and_make_async_post_calls(env, gen, api_method, sequence = None):
    return make_async_post_calls(env,
        prepare_calls_list(env, gen, api_method, sequence))

def test_initial_merge(env):
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getResourceParams'),
          ('get', 'ec2', 'getMediaServersEx'),
          ('get', 'ec2', 'getCamerasEx'),
          ('get', 'ec2', 'getUsers'),
          ('get', 'ec2', 'getFullInfo') ])

    check_transaction_log(env)

def test_api_get_methods(env):
    test_api_get_methods = [
        ('get', 'ec2', 'getResourceTypes'),
        ('get', 'ec2', 'getResourceParams'),
        ('get', 'ec2', 'getMediaServers'),
        ('get', 'ec2', 'getMediaServersEx'),
        ('get', 'ec2', 'getCameras'),
        ('get', 'ec2', 'getCamerasEx'),
        ('get', 'ec2', 'getCameraHistoryItems'),
        ('get', 'ec2', 'bookmarks/tags'),
        ('get', 'ec2', 'getEventRules'),
        ('get', 'ec2', 'getUsers'),
        ('get', 'ec2', 'getVideowalls'),
        ('get', 'ec2', 'getLayouts'),
        ('get', 'ec2', 'listDirectory'),
        ('get', 'ec2', 'getStoredFile'),
        ('get', 'ec2', 'getSettings'),
        ('get', 'ec2', 'getCurrentTime'),
        ('get', 'ec2', 'getFullInfo'),
        ('get', 'ec2', 'getLicenses')
        ]
    for srv in env.servers:
        for method, api_object, api_method in test_api_get_methods:
            srv.rest_api.get_api_fn(method, api_object, api_method)()

def test_camera_data_syncronization(env, gen):
    cameras = prepare_and_make_async_post_calls(env, gen, 'saveCamera')
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getCamerasEx') ])
    prepare_and_make_async_post_calls(env, gen, 'saveCameraUserAttributes', cameras)
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getCameraUserAttributesList'),
          ('get', 'ec2', 'getFullInfo') ])
    check_transaction_log(env)

def test_user_data_syncronization(env, gen):
    prepare_and_make_async_post_calls(env, gen, 'saveUser')
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getFullInfo') ])
    check_transaction_log(env)

def test_mediaserver_data_syncronization(env, gen):
    servers = prepare_and_make_async_post_calls(env, gen, 'saveMediaServer')
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getMediaServersEx') ])
    prepare_and_make_async_post_calls(env, gen, 'saveMediaServerUserAttributes', servers)
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getMediaServersUserAttributesList'),
          ('get', 'ec2', 'getFullInfo')])

    check_transaction_log(env)

def test_resource_params_data_syncronization(env, gen):
    cameras = prepare_and_make_async_post_calls(env, gen, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, gen, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, gen, 'saveMediaServer')
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getCamerasEx'),
          ('get', 'ec2', 'getUsers'),
          ('get', 'ec2', 'getMediaServersEx')  ])

    resources = [ v[i % 3]['id'] for i, v in enumerate(zip(cameras, users, servers))]
    prepare_and_make_async_post_calls(env, gen, 'setResourceParams', resources)
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getFullInfo')])

    check_transaction_log(env)

def test_remove_resource_params_data_syncronization(env, gen):
    cameras = prepare_and_make_async_post_calls(env, gen, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, gen, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, gen, 'saveMediaServer')
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getCamerasEx'),
          ('get', 'ec2', 'getUsers'),
          ('get', 'ec2', 'getMediaServersEx')  ])

    # Need to remove different resource types (camera, user, mediaserver)
    resources = [ v[i % 3]['id'] for i, v in enumerate(zip(cameras, users, servers))]
    prepare_and_make_async_post_calls(env, gen, 'removeResource', resources)
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getFullInfo') ])

    check_transaction_log(env)

def test_resource_remove_update_conflict(env, gen):
    cameras = prepare_and_make_async_post_calls(env, gen, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, gen, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, gen, 'saveMediaServer')
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getCamerasEx'),
          ('get', 'ec2', 'getUsers'),
          ('get', 'ec2', 'getMediaServersEx')  ])

    api_methods = ['saveCamera', 'saveUser', 'saveMediaServer' ]
    # Prepare api calls list, the list contains save.../removeResource pairs.
    # Each pair has the same resource to reach a conflict.
    api_calls = []
    # Need to check different resource types (camera, user, mediaserver).
    for i, v in enumerate(zip(cameras, users, servers)):
        api_method = api_methods[i % 3]
        data = v[i % 3]
        data['name'] += '_changed'
        server_1 = env.servers[i % len(env.servers)]
        server_2 = env.servers[(i+1) % len(env.servers)]
        api_calls.append((server_1.rest_api.get_api_fn('post', 'ec2', api_method), data))
        api_calls.append((server_2.rest_api.get_api_fn('post', 'ec2', 'removeResource'), dict(id=data['id'])))

    make_async_post_calls(env, api_calls)
    check_api_calls(
        env,
        [ ('get', 'ec2', 'getFullInfo') ])

    check_transaction_log(env)


# Uncomment to simplify VMS-5657 reproducing
#
# def test_resource_remove_update_user_conflict(env, gen):
#     user_data = generator.generate_user_data(0)
#     server_1 = env.servers[0]
#     server_2 = env.servers[1]
#     server_1.rest_api.get_api_fn('post', 'ec2', 'saveUser')(**user_data)
#     check_api_calls(
#         env, [ ('get', 'ec2', 'getUsers') ])
#     user_data['name'] += '_changed'
#     api_calls = []    
#     api_calls.append((server_1.rest_api.get_api_fn('post', 'ec2', 'saveUser'), user_data))
#     api_calls.append((server_2.rest_api.get_api_fn('post', 'ec2', 'removeResource'), dict(id=user_data['id'])))
#     make_async_post_calls(env, api_calls)
#     check_api_calls(
#         env,
#         [ ('get', 'ec2', 'getFullInfo') ])
#     check_transaction_log(env)
