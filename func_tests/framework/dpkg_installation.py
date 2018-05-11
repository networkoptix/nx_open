"""Manipulate directory to which server instance is installed."""

import logging
import sys

from pathlib2 import PurePosixPath

from framework.build_info import build_info_from_text, customizations_from_paths
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.path import copy_file
from framework.os_access.ssh_access import SSHAccess
from framework.os_access.ssh_path import SSHPath
from framework.service import UpstartService

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from ConfigParser import SafeConfigParser as ConfigParser
    # noinspection PyCompatibility,PyUnresolvedReferences
    from cStringIO import StringIO as BytesIO
elif sys.version_info[:2] in {(3, 5), (3, 6)}:
    # noinspection PyCompatibility,PyUnresolvedReferences
    from configparser import ConfigParser
    # noinspection PyCompatibility,PyUnresolvedReferences
    from io import BytesIO

log = logging.getLogger(__name__)

MEDIASERVER_CONFIG_PATH = 'etc/mediaserver.conf'
MEDIASERVER_CONFIG_PATH_INITIAL = 'etc/mediaserver.conf.initial'
MEDIASERVER_LOG_PATH = 'var/log/log_file.log'
# path to .so file containing cloud host in it. Different for different server versions
MEDIASERVER_CLOUDHOST_LIB_PATH_LIST = ['lib/libnx_network.so', 'lib/libcommon.so']

MEDIASERVER_CLOUDHOST_TAG = 'this_is_cloud_host_name'
MEDIASERVER_CLOUDHOST_SIZE = 76  # MEDIASERVER_CLOUDHOST_TAG + ' ' + cloud_host + '\0' * padded up to 76 chars


class DPKGInstallation(object):
    """One of potentially multiple installations"""

    def __init__(self, ssh_access, dir, service_name):
        self.ssh_access = ssh_access  # type: SSHAccess
        self.dir = dir  # type: SSHPath
        self._bin_dir = self.dir / 'bin'
        self._var_dir = self.dir / 'var'
        self.binary = self._bin_dir / 'mediaserver-bin'
        self.config_path = self.dir / MEDIASERVER_CONFIG_PATH
        self.config_path_initial = self.dir / MEDIASERVER_CONFIG_PATH_INITIAL
        self._log_path = self.dir / MEDIASERVER_LOG_PATH
        self._key_cert_path = self.dir / 'var/ssl/cert.pem'
        self._current_cloud_host = None  # cloud_host encoded in installed binary .so file, None means unknown yet
        self.service = UpstartService(ssh_access, service_name)

    def build_info(self):
        build_info_path = self.dir.joinpath('build_info.txt')
        build_info_text = build_info_path.read_text(encoding='ascii')
        return build_info_from_text(build_info_text)

    def is_valid(self):
        paths_to_check = [
            self.dir, self._bin_dir / 'mediaserver', self.binary,
            self.config_path, self.config_path_initial]
        all_paths_exist = True
        for path in paths_to_check:
            if path.exists():
                log.info("Path %r exists.", path)
            else:
                log.warning("Path %r does not exists.", path)
                all_paths_exist = False
        return all_paths_exist

    def cleanup_var_dir(self):
        self._var_dir.rmtree(ignore_errors=True)
        self._var_dir.mkdir()

    def put_key_and_cert(self, key_and_cert):
        self._key_cert_path.parent.mkdir(parents=True, exist_ok=True)
        self._key_cert_path.write_text(key_and_cert)

    def list_core_dumps(self):
        return self._bin_dir.glob('core.*')

    def cleanup_core_files(self):
        for core_dump_path in self.list_core_dumps():
            core_dump_path.unlink()

    def restore_mediaserver_conf(self):
        self.ssh_access.run_command(['cp', self.config_path_initial, self.config_path])

    def update_mediaserver_conf(self, new_configuration):
        old_config = self.config_path.read_text(encoding='ascii')
        config = ConfigParser()
        config.optionxform = str  # Configuration is case-sensitive.
        config.readfp(BytesIO(old_config))
        for name, value in new_configuration.items():
            config.set('General', name, str(value))
        f = BytesIO()  # TODO: Should be text.
        config.write(f)
        self.config_path.write_text(f.getvalue().decode(encoding='ascii'))

    def read_log(self):
        try:
            return self._log_path.read_bytes()
        except DoesNotExist:
            return None

    def patch_binary_set_cloud_host(self, new_host):
        if self._current_cloud_host and new_host == self._current_cloud_host:
            log.debug('Mediaserver binary at %s already has %r in it', self.ssh_access, new_host)
            return
        path_to_patch = None
        data = None
        start_idx = -1
        for lib_path in MEDIASERVER_CLOUDHOST_LIB_PATH_LIST:
            path_to_patch = self.dir / lib_path
            data = path_to_patch.read_bytes()
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
            log.debug('Mediaserver binary %s at %s already has %r in it', path_to_patch, self.ssh_access, new_host)
            self._current_cloud_host = new_host
            return
        old_str_len = len(MEDIASERVER_CLOUDHOST_TAG + ' ' + old_host)
        old_padding = data[end_idx: end_idx + MEDIASERVER_CLOUDHOST_SIZE - old_str_len]
        if old_padding != '\0' * (MEDIASERVER_CLOUDHOST_SIZE - old_str_len):
            raise RuntimeError(
                "Cloud host padding error: {:d} padding characters are expected, but got only {:d}".format(
                    MEDIASERVER_CLOUDHOST_SIZE - old_str_len,
                    old_padding.rfind('\0') + 1))
        log.info(
            'Patching %s at %s with new cloud host %r (was: %r)...',
            path_to_patch, self.ssh_access, new_host, old_host)
        new_str = MEDIASERVER_CLOUDHOST_TAG + ' ' + new_host
        assert len(new_str) < MEDIASERVER_CLOUDHOST_SIZE, 'Cloud host name is too long: %r' % new_host
        padded_str = new_str + '\0' * (MEDIASERVER_CLOUDHOST_SIZE - len(new_str))
        assert len(padded_str) == MEDIASERVER_CLOUDHOST_SIZE
        new_data = data[:start_idx] + padded_str + data[start_idx + MEDIASERVER_CLOUDHOST_SIZE:]
        assert len(new_data) == len(data)
        path_to_patch.write_bytes(new_data)
        self._current_cloud_host = new_host


def find_all_installations(os_access, installation_root):
    paths_raw_output = os_access.run_command([
        'find', installation_root,
        '-mindepth', 2, '-maxdepth', 2,
        '-type', 'd', '-name', 'mediaserver'])
    paths = [
        PurePosixPath(path_str)
        for path_str in paths_raw_output.splitlines(False)]
    installed_customizations = customizations_from_paths(paths, installation_root)
    for customization in installed_customizations:
        installation_dir = installation_root / customization.installation_subdir
        installation = DPKGInstallation(os_access, installation_dir, customization.service)
        if installation.is_valid():
            yield installation
        else:
            log.error('Installation at %s is invalid.', installation.dir)


def find_deb_installation(os_access, mediaserver_deb, installation_root):
    for installation in find_all_installations(os_access, installation_root):
        if installation.build_info() == mediaserver_deb.build_info:
            return installation
    return None


def install_mediaserver(ssh_access, mediaserver_deb, reinstall=False):
    installation_root = ssh_access.Path('/opt')
    if not reinstall:
        found_installation = find_deb_installation(ssh_access, mediaserver_deb, installation_root)
        if found_installation is not None:
            return found_installation

    customization = mediaserver_deb.customization
    installation_dir = installation_root / customization.installation_subdir
    installation = DPKGInstallation(ssh_access, installation_dir, customization.service)
    remote_path = ssh_access.Path.tmp() / mediaserver_deb.path.name
    remote_path.parent.mkdir(parents=True, exist_ok=True)
    copy_file(mediaserver_deb.path, remote_path)
    ssh_access.run_sh_script(
        # language=Bash
        '''
            # Commands and dependencies for Ubuntu 14.04 (ubuntu/trusty64 from Vagrant's Atlas).
            CORE_PATTERN_FILE='/etc/sysctl.d/60-core-pattern.conf'
            echo 'kernel.core_pattern=core.%t.%p' > "$CORE_PATTERN_FILE"  # %t is timestamp, %p is pid.
            sysctl -p "$CORE_PATTERN_FILE"  # See: https://superuser.com/questions/625840
            POINT=/mnt/trusty-packages
            mkdir -p "$POINT"
            mount -t nfs -o ro "$SHARE" "$POINT"
            DEBIAN_FRONTEND=noninteractive dpkg -i "$DEB" "$POINT"/*  # GDB (to parse core dumps) and deps.
            umount "$POINT"
            cp "$CONFIG" "$CONFIG_INITIAL"
            ''',
        env={
            'DEB': remote_path,
            'CONFIG': installation.config_path,
            'CONFIG_INITIAL': installation.config_path_initial,
            'SHARE': '10.0.2.107:/data/QA/func_tests/trusty-packages',
            })
    return installation
