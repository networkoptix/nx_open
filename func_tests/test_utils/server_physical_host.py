"""Support for installing servers on physical hosts, controlled by yaml test config."""

import logging
import uuid

from test_utils.build_info import customization_from_company_name
from .os_access import SshAccessConfig, host_from_config
from .server import Server
from .server_ctl import MEDIASERVER_DIR, PhysicalHostServerCtl, SERVER_CTL_TARGET_PATH
from .server_installation import MEDIASERVER_CONFIG_PATH, MEDIASERVER_CONFIG_PATH_INITIAL, ServerInstallation
from .template_renderer import TemplateRenderer
from .utils import is_list_inst

log = logging.getLogger(__name__)


SERVER_CTL_TEMPLATE_PATH = 'server_ctl.sh.jinja2'
SERVER_CONF_TEMPLATE_PATH = 'mediaserver.conf.jinja2'
SERVER_PORT_BASE = 1700



class PhysicalInstallationHostConfig(object):

    @classmethod
    def from_dict(cls, d):
        hostname = d.get('host')
        assert hostname, '"host" is required parameter for "hosts" elements'
        if hostname == 'localhost':
            location = None
        else:
            location = SshAccessConfig.from_dict(d)
        root_dir = d.get('dir')
        assert root_dir, '"dir" is required parameter for "hosts" elements'
        limit = d.get('limit')
        lightweight_servers_limit = d.get('lightweight_servers_limit')
        return cls(
            name=d.get('name') or hostname,
            location=location,
            root_dir=root_dir,
            limit=limit,
            lightweight_servers_limit=lightweight_servers_limit,
            )

    def __init__(self, name, root_dir, location, limit=None, lightweight_servers_limit=None):
        self.name = name
        self.location = location  # None for localhost or SshAccessConfig, argument for os_access_from_config
        self.root_dir = root_dir
        self.limit = limit  # how much servers can be allocated here, None means unlimited.
        # how much lightweight servers can be allocated here. None means not a single one.
        self.lightweight_servers_limit = lightweight_servers_limit

    def __repr__(self):
        return '<%r: %r at %r, %s, lw: %s>' % (
            self.name, self.root_dir, self.location,
            'limit %d' % self.limit if self.limit else 'unlimited',
            self.lightweight_servers_limit if self.lightweight_servers_limit else 'no')


class PhysicalInstallationCtl(object):

    def __init__(self, deb_path, customization_company_name, config_list, ca):
        assert is_list_inst(config_list, PhysicalInstallationHostConfig), repr(config_list)
        self.config_list = config_list
        self.installations = [
            PhysicalInstallationHost.from_config(
                config, deb_path, customization_from_company_name(customization_company_name), ca)
            for config in config_list]
        self._available_hosts = self.installations[:]

    # will unpack [new] distributive and reinstall servers
    def reset_all_installations(self):
        for host in self.installations:
            host.set_reset_installation_flag()

    def allocate_server(self, config):
        while self._available_hosts:
            host = self._available_hosts[0]
            server = host.allocate_server(config)
            if server:
                # next time allocate on next one
                self._available_hosts = self._available_hosts[1:] + self._available_hosts[:1]
                return server
            else:
                del self._available_hosts[0]
        return None

    def release_all_servers(self):
        log.info('Releasing all physical installations')
        for host in self.installations:
            host.release_all_servers()
        self._available_hosts = self.installations[:]


class PhysicalInstallationHost(object):

    @classmethod
    def from_config(cls, config, deb_path, customization_company_name, ca):
        return cls(config.name, host_from_config(config.location), config.root_dir, config.limit,
                   deb_path, customization_company_name, ca)

    def __init__(self, name, os_access, root_dir, limit, deb_path, customization_company_name, ca):
        self.name = name
        self.os_access = os_access  # Either local or remote by SSH.
        self.root_dir = os_access.expand_path(root_dir)
        self._remote_dist_root = self.root_dir / 'dist'
        self._limit = limit
        self._deb_path = deb_path
        self._ca = ca
        self._customization_company_name = customization_company_name
        self._template_renderer = TemplateRenderer()
        self._dist_unpacked = self.os_access.dir_exists(self.unpacked_mediaserver_dir)
        self._ensure_root_dir_exists()
        self._installations = list(self._read_installations())  # ServerInstallation list
        self._available_installations = self._installations[:]  # ServerInstallation list, set up but yet unallocated
        self._allocated_server_list = []
        self.timezone = os_access.get_timezone()
        self._ensure_servers_are_stopped()  # expected initial state, we may reset_installations after this, so pids will be lost
        self._must_reset_installation = False

    @property
    def unpacked_mediaserver_dir(self):
        return self._unpacked_mediaserver_root_dir / MEDIASERVER_DIR.format(
            customization_company_name=self._customization_company_name)

    def set_reset_installation_flag(self):
        self._must_reset_installation = True

    def _reset_installations(self):
        log.info('%s: removing directory: %s', self.name, self.root_dir)
        self.os_access.rm_tree(self.root_dir, ignore_errors=True)
        self._dist_unpacked = False  # although unpacked dist is still in place, it will be reunpacked again with new deb
        self._installations = []
        self._available_installations = []

    def allocate_server(self, config):
        if self._must_reset_installation:
            self._reset_installations()
            self._must_reset_installation = False
        if self._available_installations:
            installation = self._available_installations.pop(0)
            if config.leave_initial_cloud_host:
                self._reset_installation_dir(installation)
        else:
            installation = self._create_installation()
            if not installation:
                return None
        server_port = self._installation_server_port(self._installations.index(installation))
        rest_api_url = '%s://%s:%d/' % (config.http_schema, self.os_access.hostname, server_port)
        server_ctl = PhysicalHostServerCtl(self.os_access, installation.dir)
        server = Server(config.name, self.os_access, server_ctl, installation, rest_api_url, self._ca,
                        rest_api_timeout=config.rest_api_timeout, internal_ip_port=server_port)
        self._allocated_server_list.append(server)
        return server

    def release_all_servers(self):
        for server in self._allocated_server_list:
            server.ensure_service_status(is_started=False)
        self._allocated_server_list = []
        self._available_installations = self._installations[:]

    def ensure_mediaserver_is_unpacked(self):
        if self._must_reset_installation:
            self._reset_installations()
            self._must_reset_installation = False
        if self._dist_unpacked: return
        remote_dist_path = self._remote_dist_root / self._deb_path.name
        self.os_access.mk_dir(self._remote_dist_root)
        self.os_access.put_file(self._deb_path, self._remote_dist_root)
        self.os_access.run_command(['dpkg', '--extract', remote_dist_path, self._unpacked_mediaserver_root_dir])
        assert self.os_access.dir_exists(self.unpacked_mediaserver_dir), (
            'Distributive provided was build with another customization.'
            ' Distributive customization company name is %r, but expected is %r' %
            (self.os_access.expand_glob(self._unpacked_mediaserver_root_dir / 'opt' / '*')[0].name,
             self._customization_company_name))
        self._dist_unpacked = True

    @property
    def _unpacked_mediaserver_root_dir(self):
        return self._remote_dist_root / 'unpacked'

    def _ensure_root_dir_exists(self):
        self.os_access.mk_dir(self.root_dir)

    def _read_installations(self):
        for dir in sorted(self.os_access.expand_glob(self.root_dir / 'server-*')):
            yield ServerInstallation(self.os_access, dir)

    def _ensure_servers_are_stopped(self):
        for installation in self._installations:
            server_ctl = PhysicalHostServerCtl(self.os_access, installation.dir)
            if server_ctl.get_state():
                server_ctl.set_state(is_started=False)

    def _create_installation(self):
        if self._limit and len(self._installations) >= self._limit:
            return None
        idx = len(self._installations)
        dir = self.root_dir / ('server-%03d' % (idx + 1))
        self._prepare_installation_dir(dir, self._installation_server_port(idx))
        installation = ServerInstallation(self.os_access, dir)
        self._installations.append(installation)
        return installation

    def _installation_server_port(self, installation_idx):
        return SERVER_PORT_BASE + installation_idx + 1

    def _reset_installation_dir(self, installation):
        self.os_access.rm_tree(installation.dir)
        server_port = self._installation_server_port(self._installations.index(installation))
        self._prepare_installation_dir(installation.dir, server_port)
        
    def _prepare_installation_dir(self, dir, server_port):
        self.ensure_mediaserver_is_unpacked()
        self.os_access.mk_dir(dir)
        self.os_access.run_command(['cp', '-a'] +
                                   self.os_access.expand_glob(self.unpacked_mediaserver_dir / '*', for_shell=True) +
                                   [str(dir) + '/'])
        self._write_server_ctl(dir)
        self._write_server_conf(dir, server_port)

    def _write_server_ctl(self, dir):
        contents = self._template_renderer.render(
            SERVER_CTL_TEMPLATE_PATH,
            SERVER_DIR=dir,
            )
        server_ctl_path = dir / SERVER_CTL_TARGET_PATH
        self.os_access.write_file(server_ctl_path, contents)
        self.os_access.run_command(['chmod', '+x', server_ctl_path])

    def _write_server_conf(self, dir, server_port):
        contents = self._template_renderer.render(
            SERVER_CONF_TEMPLATE_PATH,
            SERVER_DIR=dir,
            SERVER_PORT=server_port,
            SERVER_GUID=str(uuid.uuid4()),
            )
        self.os_access.write_file(dir / MEDIASERVER_CONFIG_PATH, contents)
        self.os_access.write_file(dir / MEDIASERVER_CONFIG_PATH_INITIAL, contents)
