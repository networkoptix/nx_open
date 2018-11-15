from functools import partial
import itertools
import logging

from framework.context_logger import context_logger
from framework.utils import (
    MultiFunction,
    make_threaded_async_calls,
    single,
    )
import server_api_data_generators as generator

_logger = logging.getLogger('scalability')
_create_test_data_logger = _logger.getChild('create_test_data')


CREATE_DATA_THREAD_NUMBER = 16


make_async_calls = partial(make_threaded_async_calls, CREATE_DATA_THREAD_NUMBER)


MAX_LAYOUT_ITEMS = 10


def offset_range(server_idx, count):
    for idx in range(count):
        yield (server_idx * count) + idx


def offset_enumerate(server_idx, item_list):
    for idx, item in enumerate(item_list):
        yield ((server_idx * len(item_list)) + idx, item)


def make_server_async_calls(config, layout_item_id_gen, server_idx, server):

    server_id = server.api.get_server_id()
    admin_user = single(user for user
                        in server.api.generic.get('ec2/getUsers')
                        if user['isAdmin'])
    camera_list = [
        generator.generate_camera_data(camera_id=idx + 1, parentId=server_id)
        for idx in offset_range(server_idx, config.cameras_per_server)]


    def layout_item_generator(resource_list):
        for idx in layout_item_id_gen:
            resource = resource_list[idx % len(resource_list)]
            yield generator.generate_layout_item(idx + 1, resource['id'])


    layout_item_gen = layout_item_generator(resource_list=camera_list)


    def make_layout_item_list(layout_idx):
        # layouts have 0, 1, 2, .. MAX_LAYOUT_ITEMS-1 items count
        count = layout_idx % MAX_LAYOUT_ITEMS
        return list(itertools.islice(layout_item_gen, count))


    def camera_call_generator():
        for idx, camera in offset_enumerate(server_idx, camera_list):
            camera_user = generator.generate_camera_user_attributes_data(camera)
            camera_param_list = generator.generate_resource_params_data_list(
                idx + 1, camera, list_size=config.properties_per_camera)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveCameraUserAttributes', camera_user),
                partial(server.api.generic.post, 'ec2/setResourceParams', camera_param_list),
                ])


    def other_call_generator():
        for idx in offset_range(server_idx, config.storages_per_server):
            storage = generator.generate_storage_data(storage_id=idx + 1, parentId=server_id)
            yield partial(server.api.generic.post, 'ec2/saveStorage', storage)

        # 1 layout per user + 1 for admin
        layout_idx_gen = itertools.count(server_idx * (config.users_per_server + 1))

        for idx in offset_range(server_idx, config.users_per_server):
            user = generator.generate_user_data(user_id=idx + 1)
            layout_idx = next(layout_idx_gen)
            user_layout = generator.generate_layout_data(
                layout_id=layout_idx + 1,
                parentId=user['id'],
                items=make_layout_item_list(layout_idx),
                )
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveUser', user),
                partial(server.api.generic.post, 'ec2/saveLayout', user_layout),
                ])

        layout_idx = next(layout_idx_gen)
        admin_layout = generator.generate_layout_data(
            layout_id=layout_idx + 1,
            parentId=admin_user['id'],
            items=make_layout_item_list(layout_idx),
            )
        yield partial(server.api.generic.post, 'ec2/saveLayout', admin_layout)


    return (camera_call_generator(), other_call_generator())


@context_logger(_create_test_data_logger, 'framework.http_api')
@context_logger(_create_test_data_logger, 'framework.mediaserver_api')
def create_resources_on_servers(config, server_list):
    _logger.info('Create test data for %d servers:', len(server_list))
    layout_item_id_gen = itertools.count()  # global for all layout items

    # layout calls depend on all cameras being created, so must be called strictly after them
    camera_call_list = []
    other_call_list = []
    for server_idx, server in enumerate(server_list):
        camera_calls, other_calls = make_server_async_calls(
            config, layout_item_id_gen, server_idx, server)
        camera_call_list += list(camera_calls)
        other_call_list += list(other_calls)

    make_async_calls(camera_call_list)
    make_async_calls(other_call_list)
    _logger.info('Create test data: done.')
