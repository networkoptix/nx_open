import logging

from framework.ini_config import IniConfig
from framework.installation.installation import Installation, OsNotSupported
from framework.installation.installer import Customization
from framework.installation.windows_service import WindowsService
from framework.method_caching import cached_property
from framework.os_access.path import copy_file
from framework.os_access.windows_access import WindowsAccess
from framework.os_access.windows_remoting.registry import WindowsRegistry

_logger = logging.getLogger(__name__)


class WindowsInstallation(Installation):
    """Manage installation on Windows"""

    def __init__(self, os_access, customization):  # type: (WindowsAccess, Customization) -> None
        if not isinstance(os_access, WindowsAccess):
            raise OsNotSupported(self.__class__, os_access)
        program_files_dir = os_access.Path(os_access.env_vars()['ProgramFiles'])
        system_profile_dir = os_access.Path(os_access.system_profile_dir())
        user_profile_dir = os_access.Path(os_access.env_vars()[u'UserProfile'])
        self._system_local_app_data = system_profile_dir / 'AppData' / 'Local'
        super(WindowsInstallation, self).__init__(
            os_access=os_access,
            dir=program_files_dir / customization.windows_installation_subdir,
            binary_file='mediaserver.exe',
            var_dir=self._system_local_app_data / customization.windows_app_data_subdir,
            core_dumps_dirs=[
                self._system_local_app_data,  # Crash dumps written here.
                user_profile_dir,  # Manually created with `procdump`.
                user_profile_dir / 'AppData' / 'Local' / 'Temp',  # From task manager.
                ],
            core_dump_glob='mediaserver*.dmp',
            )
        self._config_key = WindowsRegistry(os_access.winrm).key(customization.windows_registry_key)
        self._config_key_backup = WindowsRegistry(os_access.winrm).key(customization.windows_registry_key + ' Backup')
        self.windows_access = os_access

    def is_valid(self):
        if not self.binary.exists():
            _logger.info("%r does not exist", self.binary)
            return False
        return True

    @cached_property
    def service(self):
        service_name = self.identity().customization.windows_service_name
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

    def can_install(self, installer):
        return installer.platform == 'win64' and installer.path.suffix == '.msi'

    def install(self, installer):
        remote_installer_path = self._upload_installer(installer)
        remote_log_path = remote_installer_path.parent / (remote_installer_path.name + '.install.log')
        self.windows_access.winrm.run_command([remote_installer_path, '/passive', '/log', remote_log_path])
        self._backup_configuration()

    def parse_core_dump(self, path):
        symbols_path = str.format(
            r'cache*;'
            # By some obscure reason, when run via WinRM, `cdb` cannot fetch `.pdb` from Microsoft Symbol Server.
            # (Same command, copied from `procmon`, works like a charm.)
            # Hope symbols exported from DLLs will suffice.
            # r'srv*;'
            r'{build_dir}\{build}\{customization}\windows-x64\bin;'
            # r'{build_dir}\{build}\{customization}\windows-x64\bin\plugins;'
            # r'{build_dir}\{build}\{customization}\windows-x64\bin\plugins_optional;'
            ,
            build_dir=r'\\cinas\beta-builds\repository\v1\develop\vms',
            build=self.identity().version.build,
            customization=self.identity().customization.customization_name,
            )
        self.os_access.parse_core_dump(path, symbols_path=symbols_path, timeout_sec=600)

    def _restore_conf(self):
        self._config_key_backup.copy_values_to(self._config_key)

    def update_mediaserver_conf(self, new_configuration):
        self._config_key.update_values(new_configuration)

    def ini_config(self, name):
        return IniConfig(self._system_local_app_data / 'nx_ini' / (name + '.ini'))

    def _find_library(self, name):
        return self.dir / (name + '.dll')
