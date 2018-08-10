import logging
import uuid

from .custom_posix_installation import CustomPosixInstallation
from .lightweight_mediaserver import LwsInstallation
from ..os_access.path import copy_file
from ..template_renderer import TemplateRenderer

_logger = logging.getLogger(__name__)


SERVER_CTL_TEMPLATE_PATH = 'installation/server_ctl.sh.jinja2'
SERVER_CONF_TEMPLATE_PATH = 'installation/mediaserver.conf.jinja2'


class UnpackedMediaserverGroup(object):

    def __init__(self, name, posix_access, installer, root_dir, server_bind_address, base_port, lws_port_base):
        self.name = name
        self._posix_access = posix_access
        self._installer = installer
        self._root_dir = root_dir
        self._server_bind_address = server_bind_address
        self._base_port = base_port
        self._dist_root_dir = root_dir / 'dist'
        self._dist_dir = self._dist_root_dir / 'opt' / self._installer.customization.linux_subdir
        self._is_unpacked = False
        # we need to stop lws from previous tests even if current one does not use it
        self.lws = LwsInstallation.create(posix_access, root_dir / 'lws', self, self._server_bind_address, lws_port_base)
        self._installation_list = list(self._discover_existing_installations())
        self._allocated_count = 0
        self._ensure_servers_are_stopped()

    def clean(self):
        self._root_dir.ensure_empty_dir()
        self.lws.reset_identity()
        self._installation_list = []

    def _discover_existing_installations(self):
        if not self._root_dir.exists():
            return
        index = 0
        while True:
            dir = self._installation_dir(index)
            if not dir.exists():
                break
            yield CopyInstallation(self._posix_access, dir, self, self._server_bind_address)
            index += 1

    def _installation_dir(self, index):
        return self._root_dir / 'server-{:03d}'.format(index)

    # although base port may change from previous test run, we don't need to know it to stop servers
    def _ensure_servers_are_stopped(self):
        self.lws.ensure_server_is_stopped()
        for installation in self._installation_list:
            installation.ensure_server_is_stopped()

    def _unpack(self):
        remote_path = self._posix_access.Path.tmp_file(self._installer.path)
        self._dist_root_dir.ensure_empty_dir()
        copy_file(self._installer.path, remote_path)
        self._posix_access.run_command(['dpkg', '--extract', remote_path, self._dist_root_dir])
        if not self._dist_dir.exists():
            raise RuntimeError(
                'Provided package was built with another customization. '
                'Expected: {}. But files in unpacked dir are: {}'.format(
                    self._installer.customization.linux_subdir,
                    self._dist_root_dir.joinpath('opt').glob('*'),
                    ),
                )

    def get_unpacked_dist_dir(self, installer):
        assert installer.identity == self._installer.identity  # should be used with same installer
        if not self._is_unpacked:
            self._unpack()
            self._is_unpacked = True
        return self._dist_dir

    def allocate(self):
        index = self._allocated_count
        server_port = self._base_port + index
        if index < len(self._installation_list):
            installation = self._installation_list[index]
            installation.server_port = server_port
        else:
            installation = CopyInstallation(
                self._posix_access,
                self._installation_dir(index),
                self,
                self._server_bind_address,
                server_port,
                )
            self._installation_list.append(installation)
        self._allocated_count += 1
        return installation

    def allocate_many(self, count):
        return [self.allocate() for i in range(count)]


class CopyInstallation(CustomPosixInstallation):
    """Install mediaserver by copying unpacked deb contents and expanding configs and scripts"""

    def __init__(self, posix_access, dir, installation_group, server_bind_address, server_port=None):
        super(CopyInstallation, self).__init__(posix_access, dir)
        self._installation_group = installation_group
        self._server_bind_address = server_bind_address
        self.server_port = server_port
        self._template_renderer = TemplateRenderer()

    def list_log_files(self):
        return super(CopyInstallation, self).list_log_files() + [
            self.dir / 'server.stderr',
            self.dir / 'server.stdout',
            ]

    def install(self, installer, force=False):
        if force or self.should_reinstall(installer):
            dist_dir = self._installation_group.get_unpacked_dist_dir(installer)
            self.dir.ensure_empty_dir()
            self.posix_access.run_command(['cp', '-a'] + dist_dir.glob('*') + [self.dir])
            self._write_control_script()
            self._write_server_conf()
            assert self.is_valid()
            self._identity = installer.identity
        else:
            # even if installation did not change, server base port could,
            # so we need to write config with (possible) new one
            self._write_server_conf()

    def ensure_server_is_stopped(self):
        if not self.is_valid():
            return
        if not self.service.is_running():
            return
        self.service.stop()

    def _write_control_script(self):
        contents = self._template_renderer.render(
            SERVER_CTL_TEMPLATE_PATH,
            SERVER_DIR=str(self.dir),
            )
        service_path = self.dir / 'server_ctl.sh'
        service_path.write_text(contents)
        self.posix_access.run_command(['chmod', '+x', service_path])

    def _write_server_conf(self):
        contents = self._template_renderer.render(
            SERVER_CONF_TEMPLATE_PATH,
            SERVER_DIR=str(self.dir),
            BIND_ADDRESS=self._server_bind_address,
            SERVER_PORT=self.server_port,
            SERVER_GUID=str(uuid.uuid4()),
            )
        self._config.write_text(contents)
        self._config_initial.write_text(contents)
