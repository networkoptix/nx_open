import itertools
import logging
import time
from multiprocessing.dummy import Pool as ThreadPool

import pytest

import server_api_data_generators as generator
import transaction_log
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.merging import merge_systems
from framework.utils import SimpleNamespace, datetime_utc_now

_logger = logging.getLogger(__name__)


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
            self.__resources.setdefault(server.api.get_server_id(), []).append(
                generator.get_resource_id(resource))

    def get_resources_by_server(self, server):
        if server:
            return self.__resources[server.api.get_server_id()]
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


@pytest.fixture()
def env(system, layout_file):
    return SimpleNamespace(
        one=system['first'],
        two=system['second'],
        servers=system.values(),
        test_size=DEFAULT_TEST_SIZE,
        thread_number=DEFAULT_THREAD_NUMBER,
        system_is_merged='direct-no_merge.yaml' not in layout_file,
        resource_generators=resource_generators(),
        )


def wait_entity_merge_done(servers, endpoint):
    _logger.info('TEST for %s:', endpoint)
    start_time = datetime_utc_now()
    while True:
        result_expected = servers[0].api.generic.get(endpoint)

        def check(_servers, _result_expected):
            for srv in _servers:
                _result = srv.api.generic.get(endpoint)
                if _result != _result_expected:
                    return srv, _result
            return None

        result = check(servers[1:], result_expected)
        if not result:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            _logger.error("'%s' was not synchronized in %s: '%r' and '%r'" % (
                endpoint, MEDIASERVER_MERGE_TIMEOUT, servers[0], result[0]))
            assert result[1] == result_expected
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.)


def check_api_calls(env, calls):
    for endpoint in calls:
        wait_entity_merge_done(env.servers, endpoint)


def check_transaction_log(env):
    _logger.info('TEST for GET ec2.getTransactionLog:')

    def servers_to_str(servers):
        return ', '.join("%r" % s for s in servers)

    def transactions_to_str(_transactions):
        return '\n  '.join("%s: [%s]" % (k, servers_to_str(v)) for k, v in _transactions.items())

    start_time = datetime_utc_now()
    while True:
        srv_transactions = {}
        for srv in env.servers:
            transactions = transaction_log.transactions_from_json(
                srv.api.generic.get('ec2/getTransactionLog'))
            for t in transactions:
                srv_transactions.setdefault(t, []).append(srv)
        unmatched_transactions = {t: l for t, l in srv_transactions.items()
                                  if len(l) != len(env.servers)}
        if not unmatched_transactions:
            return
        if datetime_utc_now() - start_time >= MEDIASERVER_MERGE_TIMEOUT:
            assert False, "Unmatched transaction:\n  %s" % transactions_to_str(unmatched_transactions)
        time.sleep(MEDIASERVER_MERGE_TIMEOUT.total_seconds() / 10.)


def server_api_post(post_call_data):
    server, api_method, data = post_call_data
    return server.api.generic.post('ec2/' + api_method, data)


def merge_system_if_unmerged(env):
    if env.system_is_merged:
        return
    assert len(env.servers) >= 2
    for i in range(len(env.servers) - 1):
        merge_systems(env.servers[i+1], env.servers[i])
        env.system_is_merged = True


def get_servers_admins(env):
    """Return list of pairs (mediaserver, user data).

    Get admin users from all tested servers
    """
    admins = []
    for server in env.servers:
        users = server.api.generic.get('ec2/getUsers')
        admins += [(server, u) for u in users if u['isAdmin']]
    return admins


def get_server_by_index(env, i):
    """Return Mediaserver object."""
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
    return make_async_post_calls(
        env,
        prepare_call_list(env, api_method, sequence))


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml'])
def test_initial_merge(env):
    check_api_calls(
        env,
        ['ec2/getResourceParams',
         'ec2/getMediaServersEx',
         'ec2/getCamerasEx',
         'ec2/getUsers',
         'ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml'])
def test_api_get_methods(env):
    test_api_get_methods = [
        'ec2/getResourceTypes',
        'ec2/getResourceParams',
        'ec2/getMediaServers',
        'ec2/getMediaServersEx',
        'ec2/getCameras',
        'ec2/getCamerasEx',
        'ec2/getCameraHistoryItems',
        'ec2/bookmarks/tags',
        'ec2/getEventRules',
        'ec2/getUsers',
        'ec2/getVideowalls',
        'ec2/getLayouts',
        'ec2/listDirectory',
        'ec2/getStoredFile',
        'ec2/getSettings',
        'ec2/getFullInfo',
        'ec2/getLicenses'
        ]
    for srv in env.servers:
        for endpoint in test_api_get_methods:
            srv.api.generic.get(endpoint)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_camera_data_synchronization(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    prepare_and_make_async_post_calls(env, 'saveCameraUserAttributes', cameras)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getCamerasEx',
         'ec2/getCameraUserAttributesList',
         'ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_user_data_synchronization(env):
    prepare_and_make_async_post_calls(env, 'saveUser')
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_mediaserver_data_synchronization(env):
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    prepare_and_make_async_post_calls(env, 'saveMediaServerUserAttributes', servers)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getMediaServersEx',
         'ec2/getMediaServersUserAttributesList',
         'ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_storage_data_synchronization(env):
    servers = [env.servers[i % len(env.servers)] for i in range(env.test_size)]
    server_with_guid_list = map(lambda s: (s, s.api.get_server_id()), servers)
    prepare_and_make_async_post_calls(env, 'saveStorage', server_with_guid_list)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getStorages',
         'ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_resource_params_data_synchronization(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    resources = [v[i % 3] for i, v in enumerate(zip(cameras, users, servers))]
    prepare_and_make_async_post_calls(env, 'setResourceParams', resources)
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getResourceParams',
         'ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
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
        ['ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
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
        ['ec2/getLayouts',
         'ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml'])
def test_resource_remove_update_conflict(env):
    cameras = prepare_and_make_async_post_calls(env, 'saveCamera')
    users = prepare_and_make_async_post_calls(env, 'saveUser')
    servers = prepare_and_make_async_post_calls(env, 'saveMediaServer')
    storages = prepare_and_make_async_post_calls(env, 'saveStorage', servers)
    check_api_calls(
        env,
        ['ec2/getCamerasEx',
         'ec2/getUsers',
         'ec2/getStorages',
         'ec2/getMediaServersEx'])
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
        ['ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()
