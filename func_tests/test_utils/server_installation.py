"""Manipulate directory to which server instance is installed."""

import ConfigParser
import StringIO
import logging
import os.path

log = logging.getLogger(__name__)


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

    def __init__(self, host, dir):
        self.host = host
        self.dir = dir
        self._config_path = os.path.join(self.dir, MEDIASERVER_CONFIG_PATH)
        self._config_path_initial = os.path.join(self.dir, MEDIASERVER_CONFIG_PATH_INITIAL)
        self._log_path = os.path.join(self.dir, MEDIASERVER_LOG_PATH)
        self._key_cert_path = os.path.join(self.dir, MEDIASERVER_KEY_CERT_PATH)
        self._current_cloud_host = None  # cloud_host encoded in currently installed binary .so file, None means unknown yet

    def cleanup_var_dir(self):
        self.host.run_command(['rm', '-rf', os.path.join(self.dir, MEDIASERVER_VAR_PATH, '*')])

    def put_key_and_cert(self, key_and_cert):
        self.host.write_file(os.path.join(self.dir, MEDIASERVER_KEY_CERT_PATH), key_and_cert)

    def list_core_files(self):
        return self.host.expand_glob(os.path.join(self.dir, 'bin/*core*'))

    def cleanup_core_files(self):
        for path in self.list_core_files():
            self.host.run_command(['rm', path])

    def reset_config(self, **kw):
        self.host.run_command(['cp', self._config_path_initial, self._config_path])
        self.change_config(**kw)

    def change_config(self, **kw):
        old_config = self.host.read_file(self._config_path)
        new_config = self._modify_config(old_config, **kw)
        self.host.write_file(self._config_path, new_config)

    @staticmethod
    def _modify_config(old_config, **kw):
        config = ConfigParser.SafeConfigParser()
        config.optionxform = str  # make it case-sensitive, server treats it this way (yes, this is a bug)
        config.readfp(StringIO.StringIO(old_config))
        for name, value in kw.items():
            config.set('General', name, unicode(value))
        f = StringIO.StringIO()
        config.write(f)
        return f.getvalue()

    def get_log_file(self):
        if self.host.file_exists(self._log_path):
            return self.host.read_file(self._log_path)
        else:
            return None

    def patch_binary_set_cloud_host(self, new_host):
        if self._current_cloud_host and new_host == self._current_cloud_host:
            log.debug('Server binary at %s already has %r in it', self.host, new_host)
            return
        for lib_path in MEDIASERVER_CLOUDHOST_LIB_PATH_LIST:
            path_to_patch = os.path.join(self.dir, lib_path)
            data = self.host.read_file(path_to_patch)
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
        old_host = data[start_idx + len(MEDIASERVER_CLOUDHOST_TAG) + 1 : end_idx]
        if self._current_cloud_host:
            assert old_host == self._current_cloud_host, repr((old_host, self._current_cloud_host))
        if new_host == old_host:
            log.debug('Server binary %s at %s already has %r in it', path_to_patch, self.host, new_host)
            self._current_cloud_host = new_host
            return
        old_str_len =  len(MEDIASERVER_CLOUDHOST_TAG + ' ' + old_host)
        old_padding = data[end_idx : end_idx + MEDIASERVER_CLOUDHOST_SIZE - old_str_len]
        assert old_padding == '\0' * (MEDIASERVER_CLOUDHOST_SIZE - old_str_len), (
            'Cloud host padding error: %d padding characters are expected, but got only %d' % (
            MEDIASERVER_CLOUDHOST_SIZE - old_str_len, old_padding.rfind('\0') + 1))
        log.info('Patching %s at %s with new cloud host %r (was: %r)...', path_to_patch, self.host, new_host, old_host)
        new_str = MEDIASERVER_CLOUDHOST_TAG + ' ' + new_host
        assert len(new_str) < MEDIASERVER_CLOUDHOST_SIZE, 'Cloud host name is too long: %r' % new_host
        padded_str = new_str + '\0' * (MEDIASERVER_CLOUDHOST_SIZE - len(new_str))
        assert len(padded_str) == MEDIASERVER_CLOUDHOST_SIZE
        new_data = data[:start_idx] + padded_str + data[start_idx + MEDIASERVER_CLOUDHOST_SIZE:]
        assert len(new_data) == len(data)
        self.host.write_file(path_to_patch, new_data)
        self._current_cloud_host = new_host
