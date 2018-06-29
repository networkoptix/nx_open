import logging
import uuid

from ..os_access.path import copy_file
from ..method_caching import cached_property
from ..template_renderer import TemplateRenderer
from .upstart_service import LinuxAdHocService
from .deb_installation import DebInstallation

_logger = logging.getLogger(__name__)


SERVER_CTL_TEMPLATE_PATH = 'installation/server_ctl.sh.jinja2'
SERVER_CONF_TEMPLATE_PATH = 'installation/mediaserver.conf.jinja2'


class UnpackedMediaserverGroup(object):

    def __init__(self, posix_access, installer, root_dir):
        self._posix_access = posix_access
        self._installer = installer
        self._root_dir = root_dir
        self._dist_root_dir = root_dir / 'dist'
        self._dist_dir = self._dist_root_dir / 'opt' / self._installer.customization.linux_subdir
        self._is_unpacked = False

    def _ensure_unpacked(self):
        if self._is_unpacked:
            return
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
        self._is_unpacked = True

    def make_installations(self, base_port, count):
        self._ensure_unpacked()
        return [UnpackInstallation(
                    self._posix_access,
                    self._installer,
                    self._root_dir / 'server-{:03d}'.format(idx),
                    self._dist_dir,
                    base_port + idx,
                    )
                for idx in range(count)]
    
        
class UnpackInstallation(DebInstallation):
    """Install mediaserver by copying unpacket deb contents, control it by custom scripts"""

    def __init__(self, posix_access, installer, dir, dist_dir, server_port):
        super(UnpackInstallation, self).__init__(posix_access, installer, dir)
        self._dist_dir = dist_dir
        self._server_port = server_port
        self._template_renderer = TemplateRenderer()

    @cached_property
    def service(self):
        return LinuxAdHocService(self._posix_shell, self.dir)

    def install(self):
        self.dir.ensure_empty_dir()
        self.posix_access.run_command(['cp', '-a'] + self._dist_dir.glob('*') + [self.dir])
        self._write_control_script()
        self._write_server_conf()
        assert self.is_valid()

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
            SERVER_PORT=self._server_port,
            SERVER_GUID=str(uuid.uuid4()),
            )
        self._config.write_text(contents)
        self._config_initial.write_text(contents)
