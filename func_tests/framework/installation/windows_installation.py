import logging

from framework.installation.installation import Installation
from framework.installation.installer import Installer
from framework.installation.windows_service import WindowsService
from framework.method_caching import cached_property
from framework.os_access.path import copy_file
from framework.os_access.windows_access import WindowsAccess
from framework.os_access.windows_remoting.registry import WindowsRegistry

_logger = logging.getLogger(__name__)


class WindowsInstallation(Installation):
    """Manage installation on Windows"""

    def __init__(self, windows_access, identity):  # type: (WindowsAccess, InstallIdentity) -> None
        self._identity = identity
        program_files_dir = windows_access.Path(windows_access.env_vars()['ProgramFiles'])
        customization = identity.customization
        self.dir = program_files_dir / customization.windows_installation_subdir
        self.binary = self.dir / 'mediaserver.exe'
        self.info = self.dir / 'build_info.txt'
        system_profile_dir = windows_access.Path(windows_access.system_profile_dir())
        self._local_app_data_dir = system_profile_dir / 'AppData' / 'Local'
        self.var = self._local_app_data_dir / customization.windows_app_data_subdir
        self._log_file = self.var / 'log' / 'log_file.log'
        self.key_pair = self.var / 'ssl' / 'cert.pem'
        self._config_key = WindowsRegistry(windows_access.winrm).key(customization.windows_registry_key)
        self._config_key_backup = WindowsRegistry(windows_access.winrm).key(customization.windows_registry_key + ' Backup')
        self.windows_access = windows_access

    @property
    def identity(self):
        return self._identity

    @property
    def os_access(self):
        return self.windows_access

    def is_valid(self):
        if not self.binary.exists():
            _logger.info("%r does not exist", self.binary)
            return False
        return True

    @cached_property
    def service(self):
        service_name = self._identity.customization.windows_service_name
        return WindowsService(self.windows_access.winrm, service_name)

    def _upload_installer(self, installer):
        remote_path = self.os_access.Path.tmp() / installer.path.name
        if not remote_path.exists():
            remote_path.parent.mkdir(parents=True, exist_ok=True)
            copy_file(installer.path, remote_path)
        return remote_path

    def _backup_configuration(self):
        self._config_key_backup.create()  # OK if already exists.
        self._config_key.copy_values_to(self._config_key_backup)

    def install(self, installer):
        remote_installer_path = self._upload_installer(installer)
        remote_log_path = remote_installer_path.parent / (remote_installer_path.name + '.install.log')
        self.windows_access.winrm.run_command([remote_installer_path, '/passive', '/log', remote_log_path])
        self._backup_configuration()

    def list_core_dumps_from_task_manager(self):
        base_name = self.binary.stem

        def iterate_names(limit):
            yield '{}.DMP'.format(base_name)
            for index in range(2, limit + 1):
                yield '{} ({}).DMP'.format(base_name, index)

        profile_dir = self.os_access.Path(self.os_access.env_vars()[u'UserProfile'])
        temp_dir = profile_dir / 'AppData' / 'Local' / 'Temp'
        dumps = []
        for name in iterate_names(20):  # 20 is arbitrarily big number.
            path = temp_dir / name
            if not path.exists():
                break
            dumps.append(path)

    def list_core_dumps_from_mediaserver(self):
        dumps = list(self._local_app_data_dir.glob('{}_*.dmp'.format(self.binary.name)))
        return dumps

    def list_core_dumps_from_procdump(self):
        profile_dir = self.os_access.Path(self.os_access.env_vars()[u'UserProfile'])
        dumps = list(profile_dir.glob('{}_*.dmp'.format(self.binary.name)))
        return dumps

    def list_core_dumps(self):
        dumps_from_mediaserver = self.list_core_dumps_from_mediaserver()
        dumps_from_procdump = self.list_core_dumps_from_procdump()
        dumps = dumps_from_mediaserver + dumps_from_procdump
        return dumps

    def restore_mediaserver_conf(self):
        self._config_key_backup.copy_values_to(self._config_key)

    def update_mediaserver_conf(self, new_configuration):
        self._config_key.update_values(new_configuration)

    def read_log(self):
        pass
