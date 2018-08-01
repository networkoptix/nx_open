"""API/convenience wrappers for unpack/lws installation.
Idea for this is to have same code used by self_tests and usual tests
and to have less code in tests."""

from collections import namedtuple
from contextlib import contextmanager

from pathlib2 import Path

from framework.installation.lightweight_mediaserver import LwMultiServer
from framework.installation.mediaserver import Mediaserver
from framework.installation.mediaserver_factory import collect_artifacts_from_mediaserver, examine_mediaserver
from framework.installation.unpack_installation import UnpackedMediaserverGroup
from framework.os_access.ssh_access import PhysicalSshAccess
from framework.utils import flatten_list

Host = namedtuple('Host', 'name os_access dir server_bind_address server_port_base lws_port_base')


def host_from_config(host_config):
    ssh_key = Path(host_config['key_path']).read_text()
    host_name = host_config['name']
    return Host(
        name=host_name,
        os_access=PhysicalSshAccess(
            host_name,
            host_config['address'],
            host_config['username'],
            ssh_key,
            ),
        dir=host_config['dir'],
        server_bind_address=host_config['server_bind_address'],
        server_port_base=host_config['server_port_base'],
        lws_port_base=host_config.get('lws_port_base'),  # optional
        )


class UnpackMediaserverInstallationGroups(object):

    def __init__(
            self,
            artifacts_dir,
            ca,
            mediaserver_installer,
            lightweight_mediaserver_installer,
            clean,
            group_list,
            ):
        self._artifacts_dir = artifacts_dir
        self._ca = ca
        self._mediaserver_installer = mediaserver_installer
        self._lightweight_mediaserver_installer = lightweight_mediaserver_installer
        self._group_list = group_list
        self._clean = clean

    @contextmanager
    def one_allocated_server(self, server_name, system_settings, server_config=None):
        # using last one, better if it's not the same as lws
        installation = self._group_list[-1].allocate()
        server = self._make_server(installation, server_name, system_settings, server_config)
        try:
            yield server
        finally:
            self._post_process_server(server)

    @contextmanager
    def many_allocated_servers(self, count, system_settings, server_config=None):
        count_per_group = count // len(self._group_list)  # assuming it is divisible
        server_list_list = [
            self._allocate_servers_from_group(group, count_per_group, system_settings, server_config)
            for group in self._group_list]
        server_list = flatten_list(server_list_list)
        try:
            yield server_list
        finally:
            for server in server_list:
                self._post_process_server(server)

    @contextmanager
    def allocated_lws(self, server_count, merge_timeout_sec, **kw):
        group = self._group_list[0]  # assuming first group is for lws
        group.lws.install(
            self._mediaserver_installer, self._lightweight_mediaserver_installer, force=self._clean)
        group.lws.cleanup()
        group.lws.write_control_script(server_count=server_count, **kw)
        lws = LwMultiServer(group.lws)
        try:
            lws.start()
            lws.wait_until_synced(merge_timeout_sec)
            yield lws
        finally:
            self._post_process_server(lws)

    def _allocate_servers_from_group(self, group, count, system_settings, server_config):
        installation_list = group.allocate_many(count)
        return [
            self._make_server(installation, '{}-{:03d}'.format(group.name, index), system_settings, server_config)
            for index, installation in enumerate(installation_list)]

    def _make_server(self, installation, server_name, system_settings, server_config):
        installation.install(self._mediaserver_installer, force=self._clean)
        installation.cleanup(self._ca.generate_key_and_cert())
        if server_config:
            installation.update_mediaserver_conf(server_config)
        server = Mediaserver(server_name, installation, port=installation.server_port)
        try:
            server.start()
            server.api.setup_local_system(system_settings)
            return server
        except:
            self._collect_server_actifacts(server)
            raise

    def _post_process_server(self, mediaserver):
        examine_mediaserver(mediaserver)
        self._collect_server_actifacts(mediaserver)

    def _collect_server_actifacts(self, mediaserver):
        mediaserver_artifacts_dir = self._artifacts_dir / mediaserver.name
        mediaserver_artifacts_dir.ensure_empty_dir()
        collect_artifacts_from_mediaserver(mediaserver, mediaserver_artifacts_dir)



class UnpackedMediaserverFactory(object):

    def __init__(
            self,
            artifacts_dir,
            ca,
            mediaserver_installer,
            lightweight_mediaserver_installer,
            clean,
            ):
        self._artifacts_dir = artifacts_dir
        self._ca = ca
        self._mediaserver_installer = mediaserver_installer
        self._lightweight_mediaserver_installer = lightweight_mediaserver_installer
        self._clean = clean

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
            self._artifacts_dir,
            self._ca,
            self._mediaserver_installer,
            self._lightweight_mediaserver_installer,
            self._clean,
            group_list,
            )

    def _make_group(self, host):
        return UnpackedMediaserverGroup(
            name=host.name,
            posix_access=host.os_access,
            installer=self._mediaserver_installer,
            root_dir=host.os_access.Path(host.dir),
            server_bind_address=host.server_bind_address,
            base_port=host.server_port_base,
            lws_port_base=host.lws_port_base,
            )
