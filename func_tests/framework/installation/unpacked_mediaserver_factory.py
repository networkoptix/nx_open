"""API/convenience wrappers for unpack/lws installation.
Idea for this is to have same code used by self_tests and usual tests
and to have less code in tests."""

from collections import namedtuple

from pathlib2 import Path

from framework.installation.lightweight_mediaserver import LwMultiServer
from framework.installation.mediaserver import Mediaserver
from framework.installation.unpack_installation import UnpackedMediaserverGroup
from framework.merging import setup_local_system
from framework.os_access.ssh_access import PhysicalSshAccess
from framework.utils import flatten_list

Host = namedtuple('Host', 'name os_access dir server_port_base lws_port_base')


def host_from_config(host_config):
    ssh_key = Path(host_config['key_path']).read_text()
    return Host(
        name=host_config['name'],
        os_access=PhysicalSshAccess(
            host_config['address'],
            host_config['username'],
            ssh_key,
            ),
        dir=host_config['dir'],
        server_port_base=host_config['server_port_base'],
        lws_port_base=host_config.get('lws_port_base'),  # optional
        )


class UnpackMediaserverInstallationGroups(object):

    def __init__(self, ca, mediaserver_installer, lightweight_mediaserver_installer, group_list):
        self._ca = ca
        self._mediaserver_installer = mediaserver_installer
        self._lightweight_mediaserver_installer = lightweight_mediaserver_installer
        self._group_list = group_list

    def allocate_one_server(self, server_name, system_settings):
        # using last one, better if it's not the same as lws
        installation = self._group_list[-1].allocate()
        return self._make_server(installation, server_name, system_settings)

    def allocate_many_servers(self, count, system_settings):
        count_per_group = count // len(self._group_list)  # assuming it is divisible
        server_list_list = [
            self._allocate_servers_from_group(group, count_per_group, system_settings)
            for group in self._group_list]
        return flatten_list(server_list_list)

    def allocate_lws(self, server_count, merge_timeout_sec, **kw):
        group = self._group_list[0]  # assuming first group is for lws
        group.lws.install(self._mediaserver_installer, self._lightweight_mediaserver_installer)
        group.lws.write_control_script(server_count=server_count, **kw)
        lws = LwMultiServer(group.lws)
        lws.start()
        lws.wait_until_synced(merge_timeout_sec)
        return lws

    def _allocate_servers_from_group(self, group, count, system_settings):
        installation_list = group.allocate_many(count)
        return [
            self._make_server(installation, '{}-{:03d}'.format(group.name, index), system_settings)
            for index, installation in enumerate(installation_list)]

    def _make_server(self, installation, server_name, system_settings):
        installation.install(self._mediaserver_installer)
        installation.cleanup(self._ca.generate_key_and_cert())
        server = Mediaserver(server_name, installation, port=installation.server_port)
        server.start()
        setup_local_system(server, system_settings)
        return server


class UnpackedMediaserverFactory(object):

    def __init__(
            self,
            ca,
            artifact_factory,
            mediaserver_installer,
            lightweight_mediaserver_installer,
            ):
        self._ca = ca
        self._artifact_factory = artifact_factory
        self._mediaserver_installer = mediaserver_installer
        self._lightweight_mediaserver_installer = lightweight_mediaserver_installer

    def from_host_config_list(self, host_config_list):
        return self._from_host_iter(map(host_from_config, host_config_list))

    def from_vm(self, vm, dir, server_port_base, lws_port_base):
        host = Host(
            name='vm',
            os_access=vm.os_access,
            dir=dir,
            server_port_base=server_port_base,
            lws_port_base=lws_port_base,
            )
        return self._from_host_iter([host])

    def _from_host_iter(self, host_list):
        group_list = [self._make_group(host) for host in host_list]
        return UnpackMediaserverInstallationGroups(
            self._ca, self._mediaserver_installer, self._lightweight_mediaserver_installer, group_list)

    def _make_group(self, host):
        return UnpackedMediaserverGroup(
            name=host.name,
            posix_access=host.os_access,
            installer=self._mediaserver_installer,
            root_dir=host.os_access.Path(host.dir),
            base_port=host.server_port_base,
            lws_port_base=host.lws_port_base,
            )
