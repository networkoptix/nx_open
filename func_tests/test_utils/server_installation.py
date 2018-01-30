"""Manipulate directory to which server instance is installed."""

import logging
import sys

from pathlib2 import PurePosixPath

from test_utils.build_info import build_info_from_text, customizations_from_paths
from test_utils.os_access import ProcessError
from test_utils.server_ctl import VagrantBoxServerCtl
from test_utils.utils import wait_until

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from ConfigParser import ConfigParser
    # noinspection PyCompatibility,PyUnresolvedReferences
    from cStringIO import StringIO as BytesIO
elif sys.version_info[:2] in {(3, 5), (3, 6)}:
    # noinspection PyCompatibility,PyUnresolvedReferences
    from configparser import ConfigParser
    # noinspection PyCompatibility,PyUnresolvedReferences
    from io import BytesIO

log = logging.getLogger(__name__)

DEFAULT_INSTALLATION_ROOT = PurePosixPath('/opt')
MEDIASERVER_CONFIG_PATH = 'etc/mediaserver.conf'
MEDIASERVER_CONFIG_PATH_INITIAL = 'etc/mediaserver.conf.initial'
MEDIASERVER_VAR_PATH = 'var'
MEDIASERVER_LOG_PATH = 'var/log/log_file.log'
MEDIASERVER_KEY_CERT_PATH = 'var/ssl/cert.pem'
# path to .so file containing cloud host in it. Different for different server versions
MEDIASERVER_CLOUDHOST_LIB_PATH_LIST = ['lib/libnx_network.so', 'lib/libcommon.so']

MEDIASERVER_CLOUDHOST_TAG = 'this_is_cloud_host_name'
MEDIASERVER_CLOUDHOST_SIZE = 76  # MEDIASERVER_CLOUDHOST_TAG + ' ' + cloud_host + '\0' * required paddings count to 76


class ServerInstallation(object):
    """One of potentially multiple installations"""

    def __init__(self, host, dir):
        self.os_access = host
        self.dir = dir
        self._config_path = self.dir / MEDIASERVER_CONFIG_PATH
        self._config_path_initial = self.dir / MEDIASERVER_CONFIG_PATH_INITIAL
        self._log_path = self.dir / MEDIASERVER_LOG_PATH
        self._key_cert_path = self.dir / MEDIASERVER_KEY_CERT_PATH
        self._current_cloud_host = None  # cloud_host encoded in currently installed binary .so file, None means unknown yet

    def build_info(self):
        return build_info_from_text(self.os_access.read_file(self.dir / 'build_info.txt'))

    def is_valid(self):
        raw_exit_codes = self.os_access.run_command([
            'test', '-d', self.dir, ';', 'echo', '$?', ';',
            'test', '-f', self.dir / 'bin' / 'mediaserver', ';', 'echo', '$?', ';',
            'test', '-f', self.dir / 'bin' / 'mediaserver-bin', ';', 'echo', '$?', ';',
            'test', '-f', self._config_path, ';', 'echo', '$?', ';',
            'test', '-f', self._key_cert_path, ';', 'echo', '$?', ';',
            ])
        return all(code == '0' for code in raw_exit_codes.splitlines(False))

    def cleanup_var_dir(self):
        self.os_access.run_command(['rm', '-rf', self.dir / MEDIASERVER_VAR_PATH / '*'])

    def put_key_and_cert(self, key_and_cert):
        self.os_access.write_file(self.dir / MEDIASERVER_KEY_CERT_PATH, key_and_cert.encode())

    def list_core_files(self):
        return self.os_access.expand_glob(self.dir / 'bin' / '*core*')

    def cleanup_core_files(self):
        for path in self.list_core_files():
            self.os_access.run_command(['rm', path])

    def backup_config(self):
        self.os_access.run_command(['sudo', 'cp', self._config_path, self._config_path_initial])
        log.info("Initial config is saved at: %s", self._config_path_initial)

    def reset_config(self, **kw):
        self.os_access.run_command(['cp', self._config_path_initial, self._config_path])
        self.change_config(**kw)

    def change_config(self, **kw):
        old_config = self.os_access.read_file(self._config_path)
        new_config = self._modify_config(old_config, **kw)
        self.os_access.write_file(self._config_path, new_config)

    @staticmethod
    def _modify_config(old_config, **kw):
        config = ConfigParser()
        config.optionxform = str  # make it case-sensitive, server treats it this way (yes, this is a bug)
        config.readfp(BytesIO(old_config))
        for name, value in kw.items():
            config.set('General', name, value)
        f = BytesIO()
        config.write(f)
        return f.getvalue()

    def get_log_file(self):
        if self.os_access.file_exists(self._log_path):
            return self.os_access.read_file(self._log_path)
        else:
            return None

    def patch_binary_set_cloud_host(self, new_host):
        if self._current_cloud_host and new_host == self._current_cloud_host:
            log.debug('Server binary at %s already has %r in it', self.os_access, new_host)
            return
        path_to_patch = None
        data = None
        start_idx = -1
        for lib_path in MEDIASERVER_CLOUDHOST_LIB_PATH_LIST:
            path_to_patch = self.dir / lib_path
            data = self.os_access.read_file(path_to_patch)
            start_idx = data.find(MEDIASERVER_CLOUDHOST_TAG)
            if start_idx != -1:
                break  # found required file
            else:
                log.warning('Cloud host tag %r is missing from shared library %r (size: %d)',
                            MEDIASERVER_CLOUDHOST_TAG, path_to_patch, len(data))
        assert start_idx != -1, ('Cloud host tag %r is missing from shared libraries %r'
                                 % (MEDIASERVER_CLOUDHOST_TAG, MEDIASERVER_CLOUDHOST_LIB_PATH_LIST))
        end_idx = data.find('\0', start_idx)
        assert end_idx != -1
        old_host = data[start_idx + len(MEDIASERVER_CLOUDHOST_TAG) + 1: end_idx]
        if self._current_cloud_host:
            assert old_host == self._current_cloud_host, repr((old_host, self._current_cloud_host))
        if new_host == old_host:
            log.debug('Server binary %s at %s already has %r in it', path_to_patch, self.os_access, new_host)
            self._current_cloud_host = new_host
            return
        old_str_len = len(MEDIASERVER_CLOUDHOST_TAG + ' ' + old_host)
        old_padding = data[end_idx: end_idx + MEDIASERVER_CLOUDHOST_SIZE - old_str_len]
        assert old_padding == '\0' * (MEDIASERVER_CLOUDHOST_SIZE - old_str_len), (
            'Cloud host padding error: %d padding characters are expected, but got only %d' % (
                MEDIASERVER_CLOUDHOST_SIZE - old_str_len, old_padding.rfind('\0') + 1))
        log.info('Patching %s at %s with new cloud host %r (was: %r)...', path_to_patch, self.os_access, new_host, old_host)
        new_str = MEDIASERVER_CLOUDHOST_TAG + ' ' + new_host
        assert len(new_str) < MEDIASERVER_CLOUDHOST_SIZE, 'Cloud host name is too long: %r' % new_host
        padded_str = new_str + '\0' * (MEDIASERVER_CLOUDHOST_SIZE - len(new_str))
        assert len(padded_str) == MEDIASERVER_CLOUDHOST_SIZE
        new_data = data[:start_idx] + padded_str + data[start_idx + MEDIASERVER_CLOUDHOST_SIZE:]
        assert len(new_data) == len(data)
        self.os_access.write_file(path_to_patch, new_data)
        self._current_cloud_host = new_host


def find_all_installations(os_access, installation_root=DEFAULT_INSTALLATION_ROOT):
    paths_raw_output = os_access.run_command([
        'find', installation_root,
        '-mindepth', 2, '-maxdepth', 2,
        '-type', 'd', '-name', 'mediaserver'])
    paths = [
        PurePosixPath(path_str)
        for path_str in paths_raw_output.splitlines(False)]
    installed_customizations = customizations_from_paths(paths, installation_root)
    installations = [
        ServerInstallation(os_access, installation_root / customization.installation_subdir)
        for customization in installed_customizations]
    return installations


def find_deb_installation(os_access, deb, installation_root=DEFAULT_INSTALLATION_ROOT):
    for installation in find_all_installations(os_access, installation_root=installation_root):
        if installation.build_info() == deb.build_info:
            return installation
    return None


def _port_is_opened_on_server_machine(hostname, port):
    try:
        hostname.run_command(['nc', '-z', 'localhost', port])
    except ProcessError as e:
        if e.returncode == 1:
            return False
    else:
        return True


def install_media_server(os_access, deb, installation_root=DEFAULT_INSTALLATION_ROOT):
    customization = deb.customization
    remote_path = PurePosixPath('/tmp') / 'func_tests' / customization.company_name / deb.path.name
    os_access.mk_dir(remote_path.parent)
    os_access.put_file(deb.path, remote_path)
    # Commands and dependencies for Ubuntu 14.04 (ubuntu/trusty64 from Vagrant's Atlas).
    os_access.run_command([
        'DEBIAN_FRONTEND=noninteractive',  # Bypass EULA on install.
        'dpkg',
        '--install',
        '--force-depends',  # Ignore unmet dependencies, which are installed just after.
        remote_path])
    os_access.run_command([
        'apt-get',
        'update'])  # Or "Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?"
    os_access.run_command([
        'apt-get',
        '--fix-broken',  # Install dependencies left by Mediaserver.
        '--assume-yes',
        'install'])

    installation = ServerInstallation(os_access, installation_root / customization.installation_subdir)

    assert installation.is_valid
    assert VagrantBoxServerCtl(os_access, customization.service_name).is_running()  # Must run when installation ends.
    assert wait_until(lambda: _port_is_opened_on_server_machine(os_access, 7001))  # Opens after a while.

    installation.backup_config()

    # Server may crash several times during one test, all cores are kept.
    # Legend: %t is timestamp, %p is pid.
    core_pattern_path = PurePosixPath('/etc/sysctl.d/60-core-pattern.conf')
    os_access.write_file(core_pattern_path, 'kernel.core_pattern=core.%t.%p')
    os_access.run_command(['sysctl', '-p', core_pattern_path])  # See: https://superuser.com/questions/625840

    # GDB is required to create stack traces for core dumps.
    os_access.run_command(['apt-get', '--assume-yes', 'install', 'gdb'])

    return installation
