"""Lightweight mediaservers.

Lightweight mediaserver is multiple mediaserver instances running within single process,
represented as one unit test from appserver2_ut -- P2pMessageBusTest.FullConnect.
Libraries required by this binary are taken from mediaserver distribution, *-server-*.deb.
"""

from framework.mediaserver_api import GenericMediaserverApi, MediaserverApi
from .custom_posix_installation import CustomPosixInstallation
from .installer import InstallIdentity, Version, find_customization
from .mediaserver import MEDIASERVER_START_TIMEOUT
from .. import serialize
from ..os_access.exceptions import DoesNotExist
from ..os_access.path import copy_file
from ..template_renderer import TemplateRenderer
from ..waiting import wait_for_true

LWS_SYNC_CHECK_TIMEOUT_SEC = 60  # calling api/moduleInformation to check for SF_P2pSyncDone flag

LWS_BINARY_NAME = 'appserver2_ut'
LWS_CTL_TEMPLATE_PATH = 'installation/lws_ctl.sh.jinja2'
LWS_CTL_NAME = 'server_ctl.sh'


class LwsInstallation(CustomPosixInstallation):
    """Install lightweight mediaserver(s) by copying it's binary and expanding configs and scripts."""

    SERVER_INFO_PATH = 'server_info.yaml'

    @classmethod
    def create(cls, posix_access, dir, installation_group, server_bind_address, server_port_base):
        try:
            server_info_text = dir.joinpath(cls.SERVER_INFO_PATH).read_text()
        except DoesNotExist:
            identity = None
        else:
            server_info = serialize.load(server_info_text)
            version = Version(server_info['version'])
            customization = find_customization('customization_name', server_info['customization_name'])
            identity = InstallIdentity(version, customization)
        installation = cls(
            posix_access,
            dir,
            installation_group,
            server_bind_address,
            server_port_base,
            identity,
            )
        installation.ensure_server_is_stopped()
        return installation

    def __init__(self, posix_access, dir, installation_group, server_bind_address, server_port_base, identity=None):
        super(LwsInstallation, self).__init__(posix_access, dir, core_dumps_dirs=[dir / 'bin'])
        self._installation_group = installation_group
        self.server_bind_address = server_bind_address
        self.server_port_base = server_port_base
        self._lws_binary_path = self.dir / 'bin' / LWS_BINARY_NAME
        self._log_file_base = self.dir / 'lws'
        self._test_tmp_dir = self.dir / 'tmp'
        self._identity = identity  # never has value self._NOT_SET, _discover_identity is never called
        self._template_renderer = TemplateRenderer()
        self._installed_server_count = None

    @property
    def server_count(self):
        assert self._installed_server_count, "Call 'write_control_script' method first"
        return self._installed_server_count

    @property
    def paths_to_validate(self):
        return [
            self.dir,
            self.dir / self.SERVER_INFO_PATH,
            self._lws_binary_path,
            self._test_tmp_dir,
            ]

    def list_log_files(self):
        return self.dir.glob('lws*.log') + [
            self.dir / 'server.stderr',
            self.dir / 'server.stdout',
            ]

    def ensure_server_is_stopped(self):
        if not self.is_valid():
            return
        if not self.service.is_running():
            return
        self.service.stop()

    def install(self, installer, lightweight_mediaserver_installer, force=False):
        if not force and not self.should_reinstall(installer):
            return
        dist_dir = self._installation_group.get_unpacked_dist_dir(installer)
        self.dir.ensure_empty_dir()
        self._test_tmp_dir.ensure_empty_dir()
        self.posix_access.run_command(['cp', '-a'] + dist_dir.glob('*') + [self.dir])
        copy_file(lightweight_mediaserver_installer, self._lws_binary_path)
        self.posix_access.run_command(['chmod', '+x', self._lws_binary_path])
        self._identity = installer.identity  # used by following _write_server_info
        self._write_server_info()
        self.dir.joinpath(LWS_CTL_NAME).ensure_file_is_missing()
        assert self.is_valid()

    def cleanup(self):
        self.cleanup_core_dumps()
        try:
            self._log_file_base.with_suffix('.log').unlink()
        except DoesNotExist:
            pass

    def write_control_script(self, server_count, **kw):
        assert self.server_port_base, "'lws_port_base' is not defined for this host"
        assert not self._installed_server_count, "'write_control_script' was already called"
        contents = self._template_renderer.render(
            LWS_CTL_TEMPLATE_PATH,
            SERVER_DIR=str(self.dir),
            LOG_PATH_BASE=str(self._log_file_base),
            SERVER_PORT_BASE=self.server_port_base,
            TEST_TMP_DIR=self._test_tmp_dir,
            SERVER_COUNT=server_count,
            **kw)
        lws_ctl_path = self.dir / LWS_CTL_NAME
        lws_ctl_path.write_text(contents)
        self.posix_access.run_command(['chmod', '+x', lws_ctl_path])
        self._installed_server_count = server_count

    def _write_server_info(self):
        server_info_text = serialize.dump(dict(
            customization_name=self._identity.customization.customization_name,
            version=self._identity.version.as_str,
            ))
        self.dir.joinpath(self.SERVER_INFO_PATH).write_text(server_info_text)


class LwServer(object):
    """Instance of mediaserver, one of many inside single lightweight mediaservers process"""

    def __init__(self, name, address, local_port, remote_port):
        self.name = name
        self.port = remote_port
        # TODO: Better construction interface.
        self.api = MediaserverApi(GenericMediaserverApi.new(name, address, local_port))

    def __repr__(self):
        return '<LwMediaserver {} at {}>'.format(self.name, self.api.generic.http.url(''))


class LwMultiServer(object):
    """Lightweight multi-mediaserver, single process with multiple server instances inside"""

    def __init__(self, installation):
        self.installation = installation
        self.os_access = installation.os_access
        self.address = installation.os_access.port_map.remote.address
        self.server_bind_address = installation.server_bind_address
        self._server_remote_port_base = installation.server_port_base
        self._server_count = installation.server_count
        self.service = installation.service

    def __repr__(self):
        return '<LwMultiServer at {}:{} {} (#{})>'.format(
            self.address, self._server_remote_port_base, self.installation.dir, self._server_count)

    @property
    def name(self):
        return 'lws'

    @property
    def api(self):
        return self[0].api

    def __getitem__(self, index):
        remote_port = self._server_remote_port_base + index
        local_port = self.os_access.port_map.remote.tcp(remote_port)
        return LwServer(
            name='lws-{:03d}'.format(index),
            address=self.address,
            local_port=local_port,
            remote_port=remote_port,
            )

    @property
    def servers(self):
        return [self[index] for index in range(self._server_count)]

    def start(self, already_started_ok=False):
        if self.service.is_running():
            if not already_started_ok:
                raise Exception("Already started")
        else:
            self.service.start()
            wait_for_true(
                self[0].api.is_online,
                description='{} is started'.format(self),
                timeout_sec=MEDIASERVER_START_TIMEOUT.total_seconds())

    def wait_until_synced(self, timeout_sec):
        wait_for_true(
            self._is_synced, "Waiting for lightweight servers to merge between themselves", timeout_sec)

    def _is_synced(self):
        response = self[0].api.generic.get('/api/moduleInformation', timeout=LWS_SYNC_CHECK_TIMEOUT_SEC)
        return response['serverFlags'] == 'SF_P2pSyncDone'
