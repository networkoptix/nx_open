from functools import partial
import json
import itertools
import logging

import pytest

import server_api_data_generators as generator
from framework.context_logger import context_logger
from framework.data_differ import raw_differ, full_info_differ
from framework.mediaserver_sync_wait import (
    SyncWaitTimeout,
    wait_for_api_path_match,
    wait_for_servers_synced,
    )
from framework.merging import merge_systems
from framework.utils import (
    flatten_list,
    make_threaded_async_calls,
    MultiFunction,
    SimpleNamespace,
    take_some,
    )

_logger = logging.getLogger(__name__)
_dumper_logger = _logger.getChild('dumper')


TEST_SIZE = 10
THREAD_NUMBER = 8


@pytest.fixture()
def env(system, layout_file):
    return SimpleNamespace(
        one=system['first'],
        two=system['second'],
        servers=system.values(),
        system_is_merged='direct-no_merge.yaml' not in layout_file,
        )


def merge_system_if_unmerged(env):
    if env.system_is_merged:
        return
    assert len(env.servers) >= 2
    for i in range(len(env.servers) - 1):
        merge_systems(env.servers[i+1], env.servers[i])
        env.system_is_merged = True


make_async_calls = partial(make_threaded_async_calls, THREAD_NUMBER)


@pytest.fixture(autouse=True)
def dumper(artifacts_dir, env):
    yield
    with context_logger(_dumper_logger, 'framework.mediaserver_api'):
        _logger.info('Making final servers dumps:')
        for name in ['one', 'two']:
            server = getattr(env, name)
            full_info = server.api.generic.get('ec2/getFullInfo')
            transaction_log = server.api.generic.get('ec2/getTransactionLog')
            with artifacts_dir.joinpath('%s-full-info.json' % name).open('wb') as f:
                json.dump(full_info, f, indent=2)
                with artifacts_dir.joinpath('%s-transaction-log.json' % name).open('wb') as f:
                    json.dump(transaction_log, f, indent=2)


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml'])
def test_initial_merge(artifact_factory, env):
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


def wait_api_path_synced(artifact_factory, server_list, api_path, differ=raw_differ):
    try:
        wait_for_api_path_match(server_list, api_path, differ)
    except SyncWaitTimeout as e:
        e.log_and_dump_results(artifact_factory)
        raise


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml'])
def test_api_get_methods(artifact_factory, env):

    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getResourceTypes')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getResourceParams')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getMediaServers')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getMediaServersEx')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getCameras')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getCamerasEx')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getCameraHistoryItems')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/bookmarks/tags')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getEventRules')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getUsers')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getVideowalls')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getLayouts')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/listDirectory')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getStoredFile')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getSettings')
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getFullInfo', differ=full_info_differ)
    wait_api_path_synced(artifact_factory, env.servers, 'ec2/getLicenses')

    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_camera_data_synchronization(artifact_factory, env):

    def call_generator():
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]
            camera = generator.generate_camera_data(camera_id=idx+1)
            camera_user = generator.generate_camera_user_attributes_data(camera)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveCameraUserAttributes', camera_user),
                ])

    make_async_calls(call_generator())
    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_user_data_synchronization(artifact_factory, env):

    def call_generator():
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]
            data = generator.generate_user_data(user_id=idx+1)
            yield partial(server.api.generic.post, 'ec2/saveUser', data)

    make_async_calls(call_generator())
    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_mediaserver_data_synchronization(artifact_factory, env):

    def call_generator():
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]
            server_data = generator.generate_mediaserver_data(server_id=idx+1)
            server_user_attrs = generator.generate_mediaserver_user_attributes_data(server_data)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveMediaServer', server_data),
                partial(server.api.generic.post, 'ec2/saveMediaServerUserAttributes', server_user_attrs),
                ])

    make_async_calls(call_generator())
    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_storage_data_synchronization(artifact_factory, env):
    server_to_id = {server: server.api.get_server_id() for server in env.servers}

    def call_generator():
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]
            data = generator.generate_storage_data(storage_id=idx+1, parentId=server_to_id[server])
            yield partial(server.api.generic.post, 'ec2/saveStorage', data)

    make_async_calls(call_generator())
    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_resource_params_data_synchronization(artifact_factory, env):

    def call_generator():
        for idx in range(TEST_SIZE):
            server = env.servers[idx % len(env.servers)]
            camera = generator.generate_camera_data(camera_id=idx+1)
            user = generator.generate_user_data(user_id=idx+1)
            server_data = generator.generate_mediaserver_data(server_id=idx+1)
            resource = [camera, user, server_data][idx % 3]
            resource_param_list = generator.generate_resource_params_data_list(idx+1, resource, list_size=1)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveUser', user),
                partial(server.api.generic.post, 'ec2/saveMediaServer', server_data),
                partial(server.api.generic.post, 'ec2/setResourceParams', resource_param_list),
                ])

    make_async_calls(call_generator())
    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_remove_resource_params_data_synchronization(artifact_factory, env):

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
            remove_fn = partial(server.api.generic.post, 'ec2/removeResource', dict(id=resource['id']))

            yield (save_fn, remove_fn)

    save_fn_list, remove_fn_list = zip(*call_generator())
    make_async_calls(save_fn_list)
    make_async_calls(remove_fn_list)

    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


MAX_LAYOUT_ITEMS = 10


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml', 'direct-no_merge.yaml'])
def test_layout_data_synchronization(artifact_factory, env):

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
            # layouts have 0, 1, 2, .. MAX_LAYOUT_ITEMS-1 items count
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

    make_async_calls(call_generator(camera_list, user_list, server_list))

    save_fn_list, remove_fn_list = zip(*layout_call_generator(user_list, layout_item_list_list))
    make_async_calls(save_fn_list)
    layouts_to_remove_count = TEST_SIZE / 2
    make_async_calls(remove_fn_list[:layouts_to_remove_count * 3])

    merge_system_if_unmerged(env)
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()


@pytest.mark.parametrize('layout_file', ['direct-merge_toward_requested.yaml'])
def test_resource_remove_update_conflict(artifact_factory, env):

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
    make_async_calls(save_fn_list)

    wait_for_servers_synced(artifact_factory, env.servers)

    make_async_calls(change_remove_fn_list)
    
    wait_for_servers_synced(artifact_factory, env.servers)
    for server in env.servers:
        assert not server.installation.list_core_dumps()
