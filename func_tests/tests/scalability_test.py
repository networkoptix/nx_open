"""Scalability test

Measure system synchronization time.
"""

from datetime import timedelta
import json
from collections import namedtuple
from contextlib import contextmanager
import itertools
from functools import partial
import time
import threading

from contextlib2 import ExitStack
import dateutil.parser
import pytest
from netaddr import IPNetwork

from framework.data_differ import full_info_differ, transaction_log_differ
from framework.installation.mediaserver import MEDIASERVER_MERGE_TIMEOUT
from framework.mediaserver_api import MediaserverApiRequestError
from framework.mediaserver_sync_wait import (
    SyncWaitTimeout,
    wait_for_api_path_match,
    wait_until_no_transactions_from_servers,
    )
from framework.message_bus import SetResourceParamCommand, message_bus_running
from framework.merging import merge_systems
from framework.context_logger import ContextLogger, context_logger
from framework.threaded import ThreadedCall
from framework.utils import (
    bool_to_str,
    datetime_local_now,
    imerge,
    make_threaded_async_calls,
    single,
    str_to_bool,
    threadsafe_generator,
    with_traceback,
    FunctionWithDescription,
    MultiFunction,
    )
from system_load_metrics import load_host_memory_usage
import server_api_data_generators as generator

pytest_plugins = ['system_load_metrics', 'fixtures.unpacked_mediaservers']

_logger = ContextLogger(__name__)
_create_test_data_logger = _logger.getChild('create_test_data')
_post_stamp_logger = _logger.getChild('stamp')
_post_check_logger = _logger.getChild('post_check')
_dumper_logger = _logger.getChild('dumper')


SET_RESOURCE_STATUS_CMD = '202'
CREATE_DATA_THREAD_NUMBER = 16
TRANSACTION_GENERATOR_THREAD_NUMBER = 16
STALE_LIMIT_RATIO = 0.1


@pytest.fixture()
def config(test_config):
    return test_config.with_defaults(
        SERVER_COUNT=2,
        USE_LIGHTWEIGHT_SERVERS=False,
        CAMERAS_PER_SERVER=1,
        STORAGES_PER_SERVER=1,
        USERS_PER_SERVER=1,
        PROPERTIES_PER_CAMERA=5,
        TRANSACTIONS_PER_SERVER_RATE=10,
        MERGE_TIMEOUT=MEDIASERVER_MERGE_TIMEOUT,
        MESSAGE_BUS_TIMEOUT=timedelta(seconds=3),
        MESSAGE_BUS_SERVER_COUNT=5,
        TRANSACTIONS_STAGE_DURATION=timedelta(seconds=10),
        HOST_LIST=None,  # use vm servers by default
        )


# resource creation  ================================================================================

def get_server_admin(server):
    admins = [(server, u) for u in server.api.generic.get('ec2/getUsers') if u['isAdmin']]
    assert admins
    return admins[0]


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
        for idx in offset_range(server_idx, config.CAMERAS_PER_SERVER)]


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
                idx + 1, camera, list_size=config.PROPERTIES_PER_CAMERA)
            yield MultiFunction([
                partial(server.api.generic.post, 'ec2/saveCamera', camera),
                partial(server.api.generic.post, 'ec2/saveCameraUserAttributes', camera_user),
                partial(server.api.generic.post, 'ec2/setResourceParams', camera_param_list),
                ])


    def other_call_generator():
        for idx in offset_range(server_idx, config.STORAGES_PER_SERVER):
            storage = generator.generate_storage_data(storage_id=idx + 1, parentId=server_id)
            yield partial(server.api.generic.post, 'ec2/saveStorage', storage)

        # 1 layout per user + 1 for admin
        layout_idx_gen = itertools.count(server_idx * (config.USERS_PER_SERVER + 1))

        for idx in offset_range(server_idx, config.USERS_PER_SERVER):
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
def create_test_data(config, server_list):
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


# merge  ============================================================================================

def make_core_dumps(env):
    _logger.info("Producing servers core dumps.")
    for server in [env.lws] + env.real_server_list:
        if not server:
            continue  # env.lws is None for full servers
        server.make_core_dump_if_running()


def api_path_getter(server, api_path):
    try:
        # TODO: Find out whether retries are needed.
        # Formerly, request was retried 5 times regardless of error type.
        # Retry will be reintroduced if server is forgiven for 4 failures.
        return server.api.generic.get(api_path, timeout=120)
    except MediaserverApiRequestError:
        _logger.error("{} may have been deadlocked. Making core dump.", server)
        server.make_core_dump_if_running()
        raise


def pick_some_servers(server_list, required_count):
    server_count = len(server_list)
    if server_count <= required_count:
        return server_list
    else:
        return server_list[::server_count / required_count]


def wait_for_servers_synced(artifact_factory, config, env):
    wait_until_no_transactions_from_servers(
        env.real_server_list[:1], config.MESSAGE_BUS_TIMEOUT.total_seconds())
    if len(env.real_server_list) > config.MESSAGE_BUS_SERVER_COUNT:
        server_list = pick_some_servers(env.real_server_list[1:], config.MESSAGE_BUS_SERVER_COUNT)
        wait_until_no_transactions_from_servers(server_list, config.MESSAGE_BUS_TIMEOUT.total_seconds())

    def wait_for_match(api_path, differ):
        wait_for_api_path_match(env.all_server_list, api_path, differ,
                                config.MERGE_TIMEOUT.total_seconds(), api_path_getter)

    try:
        wait_for_match('ec2/getFullInfo', full_info_differ)
        wait_for_match('ec2/getTransactionLog', transaction_log_differ)
    except SyncWaitTimeout as e:
        e.log_and_dump_results(artifact_factory)
        make_core_dumps(env)
        raise


# transactions ======================================================================================


class CameraInfo(object):

    def __init__(self, camera_id, status=False):
        self.id = camera_id
        self.status = status

    @property
    def status_str(self):
        if self.status:
            return 'Online'
        else:
            return 'Offline'


@threadsafe_generator
def transaction_generator(config, server_list):

    def server_generator(server):
        server_id = server.api.get_server_id()
        camera_list = [CameraInfo(d['id'])
                       for d in server.api.generic.get('ec2/getCameras')
                       if d['parentId'] == server_id]
        if not camera_list:
            # This may happen if we use lightweight servers and this is full one.
            # Full server does not has it's own cameras in this case.
            return
        for i, camera in enumerate(camera_list):
            camera.status = bool(i % 2)

        for idx in itertools.count():

            # change every camera status
            for camera_idx, camera in enumerate(camera_list):
                camera.status = not camera.status
                description = 'to %s: status for #%d %s: %s' % (
                    server.name, camera_idx, camera.id, camera.status_str)
                params = dict(
                    id=camera.id,
                    status=camera.status_str,
                    )
                yield FunctionWithDescription(
                    partial(server.api.generic.post, 'ec2/setResourceStatus', params),
                    description,
                    )

            # set timestamp for one of cameras
            camera_idx = idx % len(camera_list)
            camera = camera_list[camera_idx]
            iso_datetime = datetime_local_now().isoformat()
            value = '{}.{:02}.{}'.format(server.name, camera_idx, iso_datetime)
            param_list = [dict(
                resourceId=camera.id,
                name='scalability-stamp',
                value=value,
                )]
            yield FunctionWithDescription(
                partial(server.api.generic.post, 'ec2/setResourceParams', param_list),
                'to %s: stamp for #%d %s: %s' % (server.name, camera_idx, camera.id, value),
                )

    return imerge(*(server_generator(server) for server in server_list))


def parse_stamp(stamp):
    server_name, camera_idx, iso_datetime = stamp.split('.', 2)
    return dateutil.parser.parse(iso_datetime)


class Pacer(object):
    '''Ensure calls from multiple threads issued at required rate'''

    INTERVAL = 10  # seconds; check and recalculate delays per INTERVAL seconds

    def __init__(self, required_rate, thread_count):
        assert required_rate, repr(required_rate)  # must not be zero
        assert thread_count, repr(thread_count)  # must not be zero
        self._required_interval_rate = required_rate * self.INTERVAL  # transactions per interval
        self._thread_count = thread_count
        self._lock = threading.Lock()
        self._interval_start = time.time()
        self._transaction_count = 0  # during last interval
        self._transactions_post_time = 0.  # accumulated time for transaction posting
        self._sleep_time = 0.
        self.stale_time = []
        _logger.debug('Pacer: interval=%s required_interval_rate=%s',
                      self.INTERVAL, self._required_interval_rate)

    @contextmanager
    def pace_call(self):
        t = time.time()
        yield
        self._transactions_post_time += time.time() - t

        time.sleep(self._sleep_time)

        with self._lock:
            self._transaction_count += 1
            if self._transaction_count < self._required_interval_rate:
                return
            # now we have posted all transactions required for our interval (1sec)
            thread_post_time = self._transactions_post_time / self._thread_count
            remaining_time = self.INTERVAL - thread_post_time
            if thread_post_time < self.INTERVAL:
                self._sleep_time = remaining_time * self._thread_count / self._transaction_count
                time_left = self.INTERVAL - (time.time() - self._interval_start)
                if time_left > 0:
                    _logger.debug('%s seconds is left to the end of this interval; sleeping.', time_left)
                    time.sleep(time_left)  # sleep rest of this interval, blocking other threads
                stale_time = 0
            else:
                self._sleep_time = 0
                stale_time = thread_post_time - self.INTERVAL
                _logger.warning(
                    ('Servers unable to process transactions at required rate (%s/%s sec),'
                     ' stalled for %s seconds') % (self._required_interval_rate, self.INTERVAL, stale_time))
            self.stale_time.append(stale_time)
            _logger.debug('Interval stats:'
                          + ' transactions_post_time=%s' % self._transactions_post_time
                          + ' remaining_time=%s' % remaining_time
                          + ' stale_time=%s' % stale_time
                          + ' new sleep_time=%s' % self._sleep_time
                          )
            self._interval_start = time.time()
            self._transactions_post_time = 0.
            self._transaction_count = 0


@contextmanager
def transactions_generated(metrics_saver, call_generator, rate, duration):
    pacer = Pacer(rate, TRANSACTION_GENERATOR_THREAD_NUMBER)
    posted_transaction_count = itertools.count()

    def issue_transaction():
        fn = next(call_generator)
        with pacer.pace_call():
            _post_stamp_logger.info('Post %s', fn.description)
            with ExitStack() as stack:
                stack.enter_context(context_logger(_post_stamp_logger, 'framework.http_api'))
                stack.enter_context(context_logger(_post_stamp_logger, 'framework.mediaserver_api'))
                fn()
                next(posted_transaction_count)

    with ExitStack() as stack:
        _logger.debug('Starting transaction generating threads.')
        for idx in range(TRANSACTION_GENERATOR_THREAD_NUMBER):
            stack.enter_context(ThreadedCall.periodic(issue_transaction, terminate_timeout_sec=30))

        yield

        _logger.debug('Stopping transaction generating threads:')
    _logger.debug('Stopping transaction generating threads: done.')

    transaction_count = next(posted_transaction_count)
    if pacer.stale_time:
        actual_rate = transaction_count / duration.total_seconds()
        stale_sum = sum(pacer.stale_time)
        stale_average = sum(pacer.stale_time) / len(pacer.stale_time)
        stale_max = max(pacer.stale_time)
        _logger.info(
            'Issued %d transactions in %s\n' % (transaction_count, duration)
            + 'actual rate: %s\n' % actual_rate
            + 'required rate: %s\n' % rate
            + 'stalled sum: %s sec\n' % stale_sum
            + 'stalled average: %s sec\n' % stale_average
            + 'stalled max: %s sec' % stale_max
            )
        metrics_saver.save('transaction_rate', actual_rate)
        metrics_saver.save('transaction_stale.sum', stale_sum)
        metrics_saver.save('transaction_stale.average', stale_average)
        metrics_saver.save('transaction_stale.max', stale_max)
        if stale_sum / duration.total_seconds() > STALE_LIMIT_RATIO:
            _logger.warning(
                'Servers was unable to process transactions at required rate (%s/sec):' % rate
                + ' stalled for %s seconds' % pacer.stale_time)


# post transactions and measure their propagation time
def run_transactions(metrics_saver, load_averge_collector, config, env):
    transaction_gen = transaction_generator(config, env.all_server_list)
    transaction_rate = config.TRANSACTIONS_PER_SERVER_RATE * len(env.all_server_list)
    with ExitStack() as stack:
        stack.enter_context(transactions_generated(
            metrics_saver, transaction_gen, transaction_rate, config.TRANSACTIONS_STAGE_DURATION))
        watched_server_list = pick_some_servers(env.all_server_list, config.MESSAGE_BUS_SERVER_COUNT)
        bus = stack.enter_context(message_bus_running(watched_server_list))
        stack.enter_context(load_averge_collector(env.os_access_set, 'transactions'))
        delay_list = []
        start = datetime_local_now()
        while datetime_local_now() - start < config.TRANSACTIONS_STAGE_DURATION:
            transaction = bus.get_transaction(timeout_sec=0.2)
            if not transaction:
                continue
            if not isinstance(transaction.command, SetResourceParamCommand):
                continue
            if not transaction.command.param.name == 'scalability-stamp':
                continue
            stamp_dt = parse_stamp(transaction.command.param.value)
            delay = datetime_local_now() - stamp_dt
            _logger.debug('Transaction propagation delay: %s', delay)
            delay_list.append(delay)
    if delay_list:
        average_delay = sum(delay_list, timedelta()) / len(delay_list)
        max_delay = max(delay_list)
        _logger.info('Transaction propagation delay: average=%s max=%s', average_delay, max_delay)
        metrics_saver.save('transaction_delay.average', average_delay)
        metrics_saver.save('transaction_delay.max', max_delay)


# ===================================================================================================

def collect_additional_metrics(metrics_saver, os_access_set, lws):
    if lws:
        reply = lws[0].api.generic.get('api/p2pStats')
        metrics_saver.save('total_bytes_sent', int(reply['totalBytesSent']))
        # for test with lightweight servers pick only hosts with lightweight servers
        os_access_set = {lws.os_access}
    for os_access in os_access_set:
        metrics = load_host_memory_usage(os_access)
        for name in 'total used free used_swap mediaserver lws'.split():
            metric_name = 'host_memory_usage.%s.%s' % (os_access.alias, name)
            metric_value = getattr(metrics, name)
            metrics_saver.save(metric_name, metric_value)


system_settings = dict(
    autoDiscoveryEnabled=bool_to_str(False),
    synchronizeTimeWithInternet=bool_to_str(False),
    )

server_config = dict(
    p2pMode=True,
    serverGuid='8e25e200-0000-0000-{group_idx:04d}-{server_idx:012d}',
    )

Env = namedtuple('Env', 'all_server_list real_server_list lws os_access_set merge_start_time')


@contextmanager
def lws_env(config, groups):
    with groups.one_allocated_server('standalone', system_settings, server_config) as server:
        with groups.allocated_lws(
                server_count=config.SERVER_COUNT - 1,  # minus one standalone
                merge_timeout_sec=config.MERGE_TIMEOUT.total_seconds(),
                CAMERAS_PER_SERVER=config.CAMERAS_PER_SERVER,
                STORAGES_PER_SERVER=config.STORAGES_PER_SERVER,
                USERS_PER_SERVER=config.USERS_PER_SERVER,
                PROPERTIES_PER_CAMERA=config.PROPERTIES_PER_CAMERA,
                ) as lws:
            merge_start_time = datetime_local_now()
            server.api.merge(lws[0].api, lws.server_bind_address, lws[0].port, take_remote_settings=True)
            yield Env(
                all_server_list=[server] + lws.servers,
                real_server_list=[server],
                lws=lws,
                os_access_set={server.os_access, lws.os_access},
                merge_start_time=merge_start_time,
                )


@with_traceback
def make_real_servers_env(config, server_list, common_net):
    # lightweight servers create data themselves
    create_test_data(config, server_list)
    merge_start_time = datetime_local_now()
    for server in server_list[1:]:
        merge_systems(server_list[0], server, accessible_ip_net=common_net)
    return Env(
        all_server_list=server_list,
        real_server_list=server_list,
        lws=None,
        os_access_set=set(server.os_access for server in server_list),
        merge_start_time=merge_start_time,
        )


@contextmanager
def unpack_env(config, groups):
    with groups.many_allocated_servers(
            config.SERVER_COUNT, system_settings, server_config) as server_list:
        yield make_real_servers_env(config, server_list, IPNetwork('0.0.0.0/0'))


@pytest.fixture(scope='session')
def two_vm_types():
    return 'linux', 'linux'


@pytest.fixture()
def vm_env(two_clean_mediaservers, config):
    for server in two_clean_mediaservers:
        server.api.setup_local_system(system_settings)
    return make_real_servers_env(config, two_clean_mediaservers, IPNetwork('10.254.0.0/16'))


@pytest.fixture()
def env(request, unpacked_mediaserver_factory, config):
    if config.HOST_LIST:
        groups = unpacked_mediaserver_factory.from_host_config_list(config.HOST_LIST)
        if config.USE_LIGHTWEIGHT_SERVERS:
            with lws_env(config, groups) as env:
                yield env
        else:
            with unpack_env(config, groups) as env:
                yield env
    else:
        yield request.getfixturevalue('vm_env')


@context_logger(_post_check_logger, 'framework.http_api')
@context_logger(_post_check_logger, 'framework.mediaserver_api')
def perform_post_checks(env):
    _logger.info('Perform test post checks:')
    if env.real_server_list[0].api.is_online():
        settings = env.real_server_list[0].api.get_system_settings()  # log final settings
        assert str_to_bool(settings['autoDiscoveryEnabled']) is False
    _logger.info('Perform test post checks: done.')


def dump_full_info(artifacts_dir, env):
    server = env.real_server_list[0]
    _logger.info('Making final dump for first server %s:', server)
    with context_logger(_dumper_logger, 'framework.mediaserver_api'):
        full_info = server.api.generic.get('ec2/getFullInfo')
    with artifacts_dir.joinpath('full-info.json').open('wb') as f:
        json.dump(full_info, f, indent=2)

def test_scalability(artifact_factory, artifacts_dir, metrics_saver, load_averge_collector, config, env):
    assert isinstance(config.MERGE_TIMEOUT, timedelta)

    try:
        with load_averge_collector(env.os_access_set, 'merge'):
            wait_for_servers_synced(artifact_factory, config, env)
        merge_duration = datetime_local_now() - env.merge_start_time

        run_transactions(metrics_saver, load_averge_collector, config, env)

        metrics_saver.save('merge_duration', merge_duration)
        collect_additional_metrics(metrics_saver, env.os_access_set, env.lws)
        dump_full_info(artifacts_dir, env)
    finally:
        perform_post_checks(env)

    for server in env.real_server_list:
        assert not server.installation.list_core_dumps()
    if env.lws:
        assert not env.lws.installation.list_core_dumps()
