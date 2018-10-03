from collections import namedtuple
from functools import partial
import json
import itertools
import logging
import time
from multiprocessing.dummy import Pool as ThreadPool

import pytest

import server_api_data_generators as generator
import transaction_log
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.merging import merge_systems
from framework.utils import SimpleNamespace, datetime_utc_now, flatten_list
_logger = logging.getLogger(__name__)


TEST_SIZE = 10
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
        SeedResourceGenerator.__init__(self, gen_fn, initial)
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
    sequence = sequence or [(None, i) for i in range(TEST_SIZE)]
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

    for i, v in enumerate(sequence[:TEST_SIZE]):
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


def make_async_calls(env, call_gen):
    failures = []

    def call(fn):
        try:
            fn()
        except:
            _logger.exception('Error calling %r:', fn)
            failures.append(None)

    pool = ThreadPool(env.thread_number)
    # convert generator to list to avoid generator-from-thread issues and races
    pool.map(call, list(call_gen))
    pool.close()
    pool.join()
    assert not failures, 'There was %d errors in generated method calls, see logs' % len(failures)


class MultiFunction(object):

    def __init__(self, fn_list):
        self._fn_list = fn_list

    def __call__(self):
        for fn in self._fn_list:
            fn()


@pytest.fixture(autouse=True)
def dumper(artifacts_dir, env):
    yield
    for name in ['one', 'two']:
        server = getattr(env, name)
        full_info = server.api.generic.get('ec2/getFullInfo')
        transaction_log = server.api.generic.get('ec2/getTransactionLog')
        with artifacts_dir.joinpath('%s-full-info.json' % name).open('wb') as f:
            json.dump(full_info, f, indent=2)
        with artifacts_dir.joinpath('%s-transaction-log.json' % name).open('wb') as f:
            json.dump(transaction_log, f, indent=2)


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

    def call_generator():
        for i in range(TEST_SIZE):
            server = env.servers[i % len(env.servers)]
            camera = generator.generate_camera_data(camera_id=i+1)
            camera_user = generator.generate_camera_user_attributes_data(camera)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveCameraUserAttributes', camera_user),
                ])

    make_async_calls(env, call_generator())
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

    def call_generator():
        for i in range(TEST_SIZE):
            server = env.servers[i % len(env.servers)]
            data = generator.generate_user_data(user_id=i+1)
            yield partial(server.api.generic.post, 'ec2/saveUser', data)

    make_async_calls(env, call_generator())
    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_mediaserver_data_synchronization(env):

    def call_generator():
        for i in range(TEST_SIZE):
            server = env.servers[i % len(env.servers)]
            server_data = generator.generate_mediaserver_data(server_id=i+1)
            server_user_attrs = generator.generate_mediaserver_user_attributes_data(server_data)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveMediaServer', server_data),
                partial(server.api.generic.post, 'ec2/saveMediaServerUserAttributes', server_user_attrs),
                ])

    make_async_calls(env, call_generator())
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
    server_to_id = {server: server.api.get_server_id() for server in env.servers}

    def call_generator():
        for i in range(TEST_SIZE):
            server = env.servers[i % len(env.servers)]
            data = generator.generate_storage_data(storage_id=i+1, parentId=server_to_id[server])
            yield partial(server.api.generic.post, 'ec2/saveStorage', data)

    make_async_calls(env, call_generator())
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

    def call_generator():
        for i in range(TEST_SIZE):
            server = env.servers[i % len(env.servers)]
            camera = generator.generate_camera_data(camera_id=i+1)
            user = generator.generate_user_data(user_id=i+1)
            server_data = generator.generate_mediaserver_data(server_id=i+1)
            resource = [camera, user, server_data][i % 3]
            resource_param_list = generator.generate_resource_params_data_list(i+1, resource, list_size=1)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveUser', user),
                partial(server.api.generic.post, 'ec2/saveMediaServer', server_data),
                partial(server.api.generic.post, 'ec2/setResourceParams', resource_param_list),
                ])

    make_async_calls(env, call_generator())
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

    def call_generator():
        for i in range(TEST_SIZE):
            server = env.servers[i % len(env.servers)]

            camera = generator.generate_camera_data(camera_id=i+1)
            user = generator.generate_user_data(user_id=i+1)
            server_data = generator.generate_mediaserver_data(server_id=i+1)
            storage = generator.generate_storage_data(storage_id=i+1, parentId=server_data['id'])

            save_fn = MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveUser', user),
                partial(server.api.generic.post, 'ec2/saveMediaServer', server_data),
                partial(server.api.generic.post, 'ec2/saveStorage', storage),
                ])

            resource = [camera, user, server_data, storage][i % 4]
            remove_fn = partial(server.api.generic.post, 'ec2/removeResource', dict(id=resource['id']))

            yield (save_fn, remove_fn)


    save_fn_list, remove_fn_list = zip(*call_generator())
    make_async_calls(env, save_fn_list)
    make_async_calls(env, remove_fn_list)

    merge_system_if_unmerged(env)
    check_api_calls(
        env,
        ['ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


MAX_LAYOUT_ITEMS = 10


def take_some(iter, count):
    for i in range(count):
        yield next(iter)


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_layout_data_synchronization(env):

    admin_user_list = [user
                       for server in env.servers
                       for user in server.api.generic.get('ec2/getUsers')
                       if user['isAdmin']]

    def call_generator(camera_list, user_list, server_list):
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]
            yield partial(server.api.generic.post, 'ec2/saveCamera', camera_list[idx])
            yield partial(server.api.generic.post, 'ec2/saveUser', user_list[idx])
            yield partial(server.api.generic.post, 'ec2/saveMediaServer', server_list[idx])

    def layout_item_generator(resource_list):
        for idx in itertools.count():
            resource = resource_list[idx % len(resource_list)]
            yield generator.generate_layout_item(idx + 1, resource['id'])

    def layout_item_list_generator(item_gen):
        # 3 because item list is used by: user, admin_user and shared layouts
        for idx in range(TEST_SIZE * 3):
            count = idx % MAX_LAYOUT_ITEMS
            yield list(take_some(item_gen, count))

    def layout_call_generator(user_list, layout_item_list_list):
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]

            user = user_list[idx]
            user_layout = generator.generate_layout_data(
                layout_id=idx + 1,
                parentId=user['id'],
                items=layout_item_list_list[idx],
                )
            save_fn = partial(server.api.generic.post, 'ec2/saveLayout', user_layout)
            remove_fn = partial(server.api.generic.post, 'ec2/removeResource', dict(id=user_layout['id']))
            yield (save_fn, remove_fn)

            admin_user = admin_user_list[idx % len(admin_user_list)]
            admin_layout = generator.generate_layout_data(
                layout_id=TEST_SIZE + idx + 1,
                parentId=admin_user['id'],
                items=layout_item_list_list[TEST_SIZE + idx],
                )
            save_fn = partial(server.api.generic.post, 'ec2/saveLayout', admin_layout)
            remove_fn = partial(server.api.generic.post, 'ec2/removeResource', dict(id=admin_layout['id']))
            yield (save_fn, remove_fn)

            shared_layout = generator.generate_layout_data(
                layout_id=TEST_SIZE*2 + idx + 1,
                items=layout_item_list_list[TEST_SIZE*2 + idx],
                )
            save_fn = partial(server.api.generic.post, 'ec2/saveLayout', shared_layout)
            remove_fn = partial(server.api.generic.post, 'ec2/removeResource', dict(id=shared_layout['id']))
            yield (save_fn, remove_fn)

    camera_list = [generator.generate_camera_data(camera_id=idx+1) for idx in range(TEST_SIZE)]
    user_list = [generator.generate_user_data(user_id=idx+1) for idx in range(TEST_SIZE)]
    server_list = [generator.generate_mediaserver_data(server_id=idx+1) for idx in range(TEST_SIZE)]
    layout_item_list_list = list(layout_item_list_generator(
        layout_item_generator(resource_list=camera_list + server_list)))

    make_async_calls(env, call_generator(camera_list, user_list, server_list))

    save_fn_list, remove_fn_list = zip(*layout_call_generator(user_list, layout_item_list_list))
    make_async_calls(env, save_fn_list)
    layouts_to_remove_count = TEST_SIZE / 2
    make_async_calls(env, remove_fn_list[:layouts_to_remove_count * 3])

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

    def call_generator():
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]

            camera = generator.generate_camera_data(camera_id=idx+1)
            user = generator.generate_user_data(user_id=idx+1)
            server_data = generator.generate_mediaserver_data(server_id=idx+1)
            storage = generator.generate_storage_data(storage_id=idx+1, parentId=server_data['id'])

            save_fn = MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveUser', user),
                partial(server.api.generic.post, 'ec2/saveMediaServer', server_data),
                partial(server.api.generic.post, 'ec2/saveStorage', storage),
                ])

            resource = [camera, user, server_data, storage][idx % 4]
            api_method = [
                'ec2/saveCamera',
                'ec2/saveUser',
                'ec2/saveMediaServer',
                'ec2/saveStorage',
                ][idx % 4]
            changed_data = dict(resource, name=resource['name'] + '_changed')
            server_1 = env.servers[idx % len(env.servers)]
            server_2 = env.servers[(idx+1) % len(env.servers)]
            change_fn = partial(server_1.api.generic.post, api_method, changed_data)
            remove_fn = partial(server_2.api.generic.post, 'ec2/removeResource', dict(id=resource['id']))

            yield (save_fn, [change_fn, remove_fn])


    save_fn_list, change_remove_fn_list_list = zip(*call_generator())
    change_remove_fn_list = flatten_list(change_remove_fn_list_list)
    make_async_calls(env, save_fn_list)

    check_api_calls(
        env,
        ['ec2/getCamerasEx',
         'ec2/getUsers',
         'ec2/getStorages',
         'ec2/getMediaServersEx'])

    make_async_calls(env, change_remove_fn_list)
    
    check_api_calls(
        env,
        ['ec2/getFullInfo'])
    check_transaction_log(env)
    for server in env.servers:
        assert not server.installation.list_core_dumps()
