import logging

from framework.installation.installation import Installation
from framework.installation.installer import Customization
from framework.installation.installer import Installer
from framework.installation.windows_service import WindowsService
from framework.os_access.path import copy_file
from framework.os_access.windows_access import WindowsAccess

_logger = logging.getLogger(__name__)


class WindowsInstallation(Installation):
    def __init__(self, windows_access, installer):
        self._installer = installer  # type: Installer
        program_files_dir = windows_access.Path(windows_access.winrm.user_env_vars()['ProgramFiles'])
        customization = installer.customization  # type: Customization
        self.dir = program_files_dir / customization.windows_installation_subdir
        self.binary = self.dir / 'mediaserver.exe'
        self.info = self.dir / 'build_info.txt'
        system_profile_dir = windows_access.Path(windows_access.winrm.system_profile_dir())
        local_app_data_dir = system_profile_dir / 'AppData' / 'Local'
        self.var = local_app_data_dir / customization.windows_app_data_subdir
        self._log_file = self.var / 'log' / 'log_file.log'
        self.key_pair = self.var / 'ssl' / 'cert.pem'
        self._config_key = windows_access.winrm.registry_key(customization.windows_registry_key)
        self._config_key_backup = windows_access.winrm.registry_key(customization.windows_registry_key + ' Backup')
        self.service = WindowsService(windows_access.winrm, customization.windows_service_name)
        self.os_access = windows_access  # type: WindowsAccess

    def is_valid(self):
        if not self.binary.exists():
            _logger.info("%r does not exist", self.binary)
            return False
        return True

    def _upload_installer(self):
        remote_path = self.os_access.Path.tmp() / self._installer.path.name
        if not remote_path.exists():
            remote_path.parent.mkdir(parents=True, exist_ok=True)
            copy_file(self._installer.path, remote_path)
        return remote_path

    def _backup_configuration(self):
        values = self._config_key.list_values()
        self._config_key_backup.create()
        for value in values:
            value.copy(self._config_key_backup)

    def install(self):
        remote_installer_path = self._upload_installer()
        remote_log_path = remote_installer_path.parent / (remote_installer_path.name + '.install.log')
        self.os_access.winrm.run_command([remote_installer_path, '/passive', '/log', remote_log_path])
        self._backup_configuration()

    def list_core_dumps(self):
        base_name = self.binary.stem

        def iterate_names(limit):
            yield '{}.DMP'.format(base_name)
            for index in range(2, limit + 1):
                yield '{} ({}).DMP'.format(base_name, index)

        def iterate_existing_paths(profile_dir):
            user_temp_dir = profile_dir / 'AppData' / 'Local' / 'Temp'
            for name in iterate_names(20):  # 20 is arbitrarily big number.
                path = user_temp_dir / name
                if not path.exists():
                    break
                yield path

        user_profile_dir = self.os_access.Path(self.os_access.winrm.user_env_vars()[u'UserProfile'])
        system_profile_dir = self.os_access.Path(self.os_access.winrm.system_profile_dir())
        paths = list(iterate_existing_paths(user_profile_dir)) + list(iterate_existing_paths(system_profile_dir))
        return paths

    def restore_mediaserver_conf(self):
        pass

    def update_mediaserver_conf(self, new_configuration):
        pass

    def read_log(self):
        pass
