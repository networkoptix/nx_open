import logging
import uuid

from .. import serialize
from ..os_access.exceptions import DoesNotExist
from ..os_access.path import copy_file
from ..method_caching import cached_property
from ..template_renderer import TemplateRenderer
from .upstart_service import LinuxAdHocService
from .deb_installation import DebInstallation

_logger = logging.getLogger(__name__)


SERVER_CTL_TEMPLATE_PATH = 'installation/server_ctl.sh.jinja2'
SERVER_CONF_TEMPLATE_PATH = 'installation/mediaserver.conf.jinja2'


class UnpackedMediaserverGroup(object):

    def __init__(self, posix_access, installer, root_dir, base_port):
        self._posix_access = posix_access
        self._installer = installer
        self._root_dir = root_dir
        self._base_port = base_port
        self._dist_root_dir = root_dir / 'dist'
        self._dist_dir = self._dist_root_dir / 'opt' / self._installer.customization.linux_subdir
        self._is_unpacked = False
        self._allocated_count = 0
        self._installation_map = {}  # index -> CopyInstallation
        # base port may change between runs due to framework changes, so we need to store them separately
        self._allocated_port_set = set()
        self._discover_existing_installations()
        self._ensure_servers_are_stopped()

    def _discover_existing_installations(self):
        if not self._root_dir.exists():
            return
        for dir in self._root_dir.glob('server-*'):
            try:
                installation = CopyInstallation.from_existing_dir(self._posix_access, dir, self)
            except DoesNotExist as e:
                _logger.info('Skip installation dir (server info is missing): %s', e)
                continue
            self._installation_map[installation.index] = installation
            self._allocated_port_set.add(installation.server_port)

    def _ensure_servers_are_stopped(self):
        for installation in self._installation_map.values():
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

    def _allocate_port(self):
        port = self._base_port
        while port in self._allocated_port_set:
            port += 1
        self._allocated_port_set.add(port)
        return port

    def allocate(self):
        port = self._allocate_port()
        index = self._allocated_count
        installation = self._installation_map.get(index)
        if not installation:
            installation = self._make_installation(index, port)
            self._installation_map[index] = installation
        self._allocated_count += 1
        return installation

    def allocate_many(self, count):
        return [self.allocate() for i in range(count)]

    def _make_installation(self, index, port):
        return CopyInstallation(
            self._posix_access,
            self._root_dir / 'server-{:03d}'.format(index),
            self,
            index,
            port,
            )
    
        
class CopyInstallation(DebInstallation):
    """Install mediaserver by copying unpacked deb contents, control it using custom scripts"""

    @classmethod
    def from_existing_dir(cls, posix_access, dir, installation_group):
        server_info_text = cls._server_info_path(dir).read_text()
        server_info = serialize.load(server_info_text)
        return cls(posix_access, dir, installation_group, server_info['index'], server_info['server_port'])

    def __init__(self, posix_access, dir, installation_group, index, server_port):
        super(CopyInstallation, self).__init__(posix_access, dir)
        self._installation_group = installation_group
        self.index = index
        self.server_port = server_port
        self._template_renderer = TemplateRenderer()

    @staticmethod
    def _server_info_path(dir):
        return dir / 'server_info.yaml'

    @cached_property
    def service(self):
        return LinuxAdHocService(self._posix_shell, self.dir)

    def install(self, installer):
        if not self.should_reinstall(installer):
            return
        dist_dir = self._installation_group.get_unpacked_dist_dir(installer)
        self.dir.ensure_empty_dir()
        self.posix_access.run_command(['cp', '-a'] + dist_dir.glob('*') + [self.dir])
        self._write_control_script()
        self._write_server_conf()
        self._write_server_info()
        assert self.is_valid()
        self._identity = installer.identity

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
            SERVER_PORT=self.server_port,
            SERVER_GUID=str(uuid.uuid4()),
            )
        self._config.write_text(contents)
        self._config_initial.write_text(contents)

    def _write_server_info(self):
        server_info_text = serialize.dump(dict(
            index=self.index,
            server_port=self.server_port,
            ))
        self._server_info_path(self.dir).write_text(server_info_text)
