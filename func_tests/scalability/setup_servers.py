#!/usr/bin/env python2

# Setup (install and start) mediaservers; create test data and merge them into a system.

import argparse
from collections import namedtuple
from contextlib import contextmanager
import logging

from contextlib2 import ExitStack
from netaddr import IPNetwork
import yaml

from framework.ca import CA
from framework.config import str_to_timedelta
from framework.installation.installer import InstallerSet
from framework.installation.lightweight_mediaserver import LWS_BINARY_NAME
from framework.installation.unpacked_mediaserver_factory import UnpackedMediaserverFactory
from framework.mediaserver_sync_wait import SyncWaitTimeout
from framework.merging import merge_systems
from framework.os_access.local_path import LocalPath
from framework.utils import bool_to_str, datetime_local_now, single, with_traceback
from create_resources import create_resources_on_servers
from resource_synchronization import wait_for_servers_synced
from cmdline_logging import init_logging

_logger = logging.getLogger('scalability.setup_servers')


MESSAGE_BUS_SERVER_COUNT = 5


class SetupConfig(namedtuple('_SetupConfig', [
    'host_list',
    'use_lightweight_servers',
    'server_count',
    'merge_timeout',
    'cameras_per_server',
    'storages_per_server',
    'users_per_server',
    'properties_per_camera',
    ])):

    @classmethod
    def from_test_config(cls, test_config, use_lightweight_servers=False, server_count=None):
        return cls(
            host_list=test_config['host_list'],
            use_lightweight_servers=use_lightweight_servers,
            server_count=server_count or test_config['server_count'],
            merge_timeout=str_to_timedelta(test_config['merge_timeout']),
            cameras_per_server=test_config['cameras_per_server'],
            storages_per_server=test_config['storages_per_server'],
            users_per_server=test_config['users_per_server'],
            properties_per_camera=test_config['properties_per_camera'],
            )

    @classmethod
    def from_pytest_config(cls, test_config):
        return cls(
            host_list=test_config.HOST_LIST,
            use_lightweight_servers=test_config.USE_LIGHTWEIGHT_SERVERS,
            server_count=test_config.SERVER_COUNT,
            merge_timeout=test_config.MERGE_TIMEOUT,
            cameras_per_server=test_config.CAMERAS_PER_SERVER,
            storages_per_server=test_config.STORAGES_PER_SERVER,
            users_per_server=test_config.USERS_PER_SERVER,
            properties_per_camera=test_config.PROPERTIES_PER_CAMERA,
            )
    

system_settings = dict(
    autoDiscoveryEnabled=bool_to_str(False),
    autoDiscoveryResponseEnabled=bool_to_str(False),
    synchronizeTimeWithInternet=bool_to_str(False),
    timeSynchronizationEnabled=bool_to_str(False),
    )

unpacked_server_config = dict(
    p2pMode=True,
    serverGuid='8e25e200-0000-0000-{group_idx:04d}-{server_idx:012d}',
    )

Env = namedtuple('Env', 'all_server_list real_server_list lws os_access_set merge_start_time')


@with_traceback
def make_real_servers_env(setup_config, server_list, common_net):
    # lightweight servers create data themselves
    create_resources_on_servers(setup_config, server_list)
    merge_start_time = datetime_local_now()
    for server in server_list[1:]:
        merge_systems(server_list[0], server,
                      accessible_ip_net=common_net,
                      timeout_sec=setup_config.merge_timeout.total_seconds())
    return Env(
        all_server_list=server_list,
        real_server_list=server_list,
        lws=None,
        os_access_set=set(server.os_access for server in server_list),
        merge_start_time=merge_start_time,
        )


@contextmanager
def servers_set_up(unpacked_mediaserver_factory, setup_config, create_data_for_lws=False):
    groups = unpacked_mediaserver_factory.from_host_config_list(setup_config.host_list)
    with ExitStack() as stack:
        if setup_config.use_lightweight_servers:
            if create_data_for_lws:
                lws_config = dict(
                    CAMERAS_PER_SERVER=0,
                    STORAGES_PER_SERVER=0,
                    USERS_PER_SERVER=0,
                    PROPERTIES_PER_CAMERA=0,
                    )
            else:
                lws_config = dict(
                    CAMERAS_PER_SERVER=setup_config.cameras_per_server,
                    STORAGES_PER_SERVER=setup_config.storages_per_server,
                    USERS_PER_SERVER=setup_config.users_per_server,
                    PROPERTIES_PER_CAMERA=setup_config.properties_per_camera,
                    )
            lws = stack.enter_context(groups.allocated_lws(
                server_count=setup_config.server_count - 1,  # minus one standalone
                merge_timeout_sec=setup_config.merge_timeout.total_seconds(),
                **lws_config
                ))
            server = stack.enter_context(
                groups.one_allocated_server('standalone', system_settings, unpacked_server_config))
            all_server_list = [server] + lws.servers
            if create_data_for_lws:
                create_resources_on_servers(setup_config, all_server_list)
            merge_start_time = datetime_local_now()
            server.api.merge(lws[0].api, lws.server_bind_address, lws[0].port,
                             take_remote_settings=True,
                             timeout_sec=setup_config.merge_timeout.total_seconds())
            env = Env(
                all_server_list=all_server_list,
                real_server_list=[server],
                lws=lws,
                os_access_set={server.os_access, lws.os_access},
                merge_start_time=merge_start_time,
                )
        else:
            server_list = stack.enter_context(groups.many_allocated_servers(
                setup_config.server_count, system_settings, unpacked_server_config))
            env = make_real_servers_env(setup_config, server_list, IPNetwork('0.0.0.0/0'))

        yield env


def _setup_main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logging-config', default='default.yaml',
                        help="Configuration file for logging, in yaml format. Relative to logging-config dir if relative.")
    parser.add_argument('--clean', action='store_true', help="Clean everything first.")
    parser.add_argument('--lws', action='store_true', help="Use lightweight servers instead of full ones.")
    parser.add_argument('--base-port', type=int, default=7001, help="Base mediaserver port, default is 7001")
    parser.add_argument('--server-count', type=int, help="Total mediaserver count to setup.")
    parser.add_argument('--create-data-for-lws', action='store_true',
                        help=("Use same resource creation logic for lws as for full servers. "
                              "Do not use lws internal logic for data creation."))
    parser.add_argument('work_dir', type=LocalPath, help="Almost all files generated by tests placed there.")
    parser.add_argument('mediaserver_installers_dir', type=LocalPath,
                        help="Directory with installers of same version and customization.")
    parser.add_argument('config', type=LocalPath,
                        help="Path to yaml configuration file. Same configuration as used by tests.")
    
    args = parser.parse_args()

    init_logging(args.logging_config)

    config = yaml.load(args.config.read_text())
    scalability_config = config['tests']['test_scalability']
    setup_config = SetupConfig.from_test_config(scalability_config, args.lws, args.server_count)
    mediaserver_installer_set = InstallerSet(args.mediaserver_installers_dir)
    mediaserver_installer = single(installer for installer in mediaserver_installer_set.installers
                                   if installer.platform_variant == 'ubuntu'
                                   and installer.arch == 'x64'
                                   and installer.component == 'server')
    unpacked_mediaserver_factory = UnpackedMediaserverFactory(
        args.work_dir / 'artifacts',
        CA(args.work_dir / 'ca', clean=args.clean),
        mediaserver_installer,
        lightweight_mediaserver_installer=args.mediaserver_installers_dir / LWS_BINARY_NAME,
        clean=args.clean,
        )
    with servers_set_up(unpacked_mediaserver_factory, setup_config, args.create_data_for_lws) as env:
        try:
            wait_for_servers_synced(
                match_server_list=env.all_server_list,
                watch_server_list=env.real_server_list,
                merge_timeout=setup_config.merge_timeout,
                message_bus_timeout=str_to_timedelta(scalability_config['message_bus_timeout']),
                message_bus_server_count=MESSAGE_BUS_SERVER_COUNT,
                )
        except SyncWaitTimeout as e:
            e.log_and_dump_results(args.work_dir / 'artifacts')
            raise
        merge_duration = datetime_local_now() - env.merge_start_time
        _logger.info("\n\n*****  Done. Merged in %s (sec).  *****\n", merge_duration)
    

if __name__ == '__main__':
    _setup_main()
