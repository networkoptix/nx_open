"""Create lightweight servers (LWS) containers implemented by appserver2_ut with requested number of servers within"""

import datetime
import logging

from requests.exceptions import ReadTimeout

from framework.mediaserver_factory import CORE_FILE_ARTIFACT_TYPE, SERVER_LOG_ARTIFACT_TYPE, TRACEBACK_ARTIFACT_TYPE
from framework.os_access.path import copy_file
from framework.rest_api import RestApi
from framework.waiting import wait_for_true
from . import utils
from .core_file_traceback import create_core_file_traceback
from .mediaserver import Mediaserver
from .service import AdHocService
from .template_renderer import TemplateRenderer
from .utils import GrowingSleep

log = logging.getLogger(__name__)


LWS_BINARY_NAME = 'appserver2_ut'
LWS_CTL_TEMPLATE_PATH = 'lws_ctl.sh.jinja2'
LWS_PORT_BASE = 3000
LWS_START_TIMEOUT = datetime.timedelta(minutes=10)  # timeout when waiting for lws become online (pingable)


class LightweightServersFactory(object):

    def __init__(self, artifact_factory, physical_installation_ctl, test_binary_path, ca):
        self._artifact_factory = artifact_factory
        self._physical_installation_ctl = physical_installation_ctl
        self._test_binary_path = test_binary_path
        physical_installation_host = self._pick_server()
        if physical_installation_host:
            self._lws_host = LightweightServersHost(
                self._artifact_factory,
                self._test_binary_path,
                physical_installation_host,
                ca)
        else:
            self._lws_host = None

    def __call__(self, server_count, **kw):
        return self._allocate(server_count, kw)

    def release(self):
        if self._lws_host:
            self._lws_host.release()

    def perform_post_checks(self):
        if self._lws_host:
            self._lws_host.perform_post_checks()

    def _pick_server(self):
        if not self._physical_installation_ctl:
            return None
        config_list = self._physical_installation_ctl.config_list
        installations_access = self._physical_installation_ctl.installations_access
        for config, host in zip(config_list, installations_access):
            if config.lightweight_servers_limit:
                log.info('Lightweight host: %s %s', config, host)
                return host
        return None

    def _allocate(self, server_count, lws_params):
        if not server_count:
            return []
        if self._lws_host:
            return list(self._lws_host.allocate(server_count, lws_params))
        else:
            log.warning('No lightweight servers are configured, but requested: %d' % server_count)
            return []


class LightweightServersInstallation(object):

    def __init__(self, host, dir, ca):
        self.os_access = host
        self.dir = dir
        self.test_tmp_dir = dir / 'tmp'
        self.log_path_base = dir / 'lws'
        self._ca = ca

    def cleanup_var_dir(self):
        self._not_supported()

    def list_core_files(self):
        return self.dir.glob('*core*')

    def cleanup_core_files(self):
        # When filename contain space, it's interpreted as several arguments.
        self.os_access.run_command([
            'rm',
            '--force',  # Ignore non-existent files.
            '--',  # Avoid confusion with options.
            self.dir / '*core*'])

    def cleanup_test_tmp_dir(self):
        self.test_tmp_dir.rmtree(ignore_errors=True)

    def get_log_file(self):
        if self.log_path_base.exists():
            return self.log_path_base.with_suffix('.log').read_bytes()
        else:
            return None

    def _not_supported(self):
        assert False, 'This operation is not supported by lightweight server'


class LightweightServer(Mediaserver):
    def __init__(self, name, os_access, service, installation, api, port=None):
        super(LightweightServer, self).__init__(name, service, installation, api, None, port=port)
        self.internal_ip_address = os_access.hostname

    def wait_until_synced(self, timeout):
        log.info('Waiting for lightweight servers to merge between themselves')
        growing_delay = GrowingSleep()
        start_time = utils.datetime_utc_now()
        while utils.datetime_utc_now() - start_time < timeout:
            try:
                # calling api/moduleInformation to check for SF_P2pSyncDone flag
                response = self.api.api.moduleInformation.GET(timeout=60)
            except ReadTimeout:
                # log.error('ReadTimeout when waiting for lws api/moduleInformation; will make core dump')
                # self.service.make_core_dump()
                raise
            if response['serverFlags'] == 'SF_P2pSyncDone':
                log.info(
                    'Lightweight servers merged between themselves in %s',
                    (utils.datetime_utc_now() - start_time))
                return
            growing_delay.sleep()
        assert False, 'Lightweight servers did not merge between themselves in %s' % timeout


class LightweightServersHost(object):

    def __init__(self, artifact_factory, test_binary_path, physical_installation_host, ca):
        self._artifact_factory = artifact_factory
        self._test_binary_path = test_binary_path
        self._physical_installation_host = physical_installation_host
        self._os_access = physical_installation_host.os_access
        self._host_name = physical_installation_host.name
        self._ca = ca
        self._installation = LightweightServersInstallation(
            self._os_access, physical_installation_host.root_dir / 'lws', self._ca)
        self._template_renderer = TemplateRenderer()
        self._lws_dir = self._installation.dir
        self.service = AdHocService(self._os_access, self._lws_dir)
        self._allocated = False
        self._first_server = None
        self._init()

    # server_count is ignored for now; all servers are allocated on first lightweight installation
    def allocate(self, server_count, lws_params):
        assert not self._allocated, 'Lightweight servers were already allocated by this test'
        pih = self._physical_installation_host
        server_dir = pih.unpacked_mediaserver_dir
        pih.ensure_mediaserver_is_unpacked()
        self._lws_dir.mkdir(exist_ok=True)
        self._cleanup_log_files()
        copy_file(self._test_binary_path, self._lws_dir)
        self._write_lws_ctl(server_dir, server_count, lws_params)
        self.service.start()
        # must be set before loop following it
        # so failure in that loop won't prevent artifacts collection from 'release' method
        self._allocated = True
        for idx in range(server_count):
            server_port = LWS_PORT_BASE + idx
            name = 'lws-%05d' % idx
            api = RestApi(name, self._os_access.hostname, server_port)
            server = LightweightServer(name, self._os_access, self.service, self._installation, api, port=server_port)
            wait_for_true(server.is_online, "{} is online after allocation".format(server))
            if not self._first_server:
                self._first_server = server
            yield server

    def release(self):
        if self._allocated:
            self._save_lws_artifacts()
        self._first_server = None
        self._allocated = False

    def _init(self):
        if self.service.is_running():
            self.service.stop()
        self._installation.cleanup_core_files()
        self._installation.cleanup_test_tmp_dir()

    def _cleanup_log_files(self):
        for log_file_path in self._installation.dir.glob('lws*.log'):
            log_file_path.unlink()

    def _write_lws_ctl(self, server_dist_dir, server_count, lws_params):
        contents = self._template_renderer.render(
            LWS_CTL_TEMPLATE_PATH,
            SERVER_DIR=self._lws_dir,
            MEDIASERVER_DIST_DIR=server_dist_dir,
            LOG_PATH_BASE=self._installation.log_path_base,
            SERVER_COUNT=server_count,
            PORT_BASE=LWS_PORT_BASE,
            TEST_TMP_DIR=self._installation.test_tmp_dir,
            **lws_params)
        lws_ctl_path = self._lws_dir / 'server_ctl.sh'
        lws_ctl_path.write_text(contents)
        self._os_access.run_command(['chmod', '+x', lws_ctl_path])

    def _save_lws_artifacts(self):
        self._check_if_server_is_online()
        self._save_lws_log()
        self._save_lws_core_files()

    def _save_lws_log(self):
        log_contents = self._installation.log_path_base.with_suffix('.log').read_text().strip()
        if log_contents:
            artifact_factory = self._artifact_factory(
                ['lws', self._host_name],
                name='lws',
                artifact_type=SERVER_LOG_ARTIFACT_TYPE)
            log_path = artifact_factory.produce_file_path().write_bytes(log_contents)
            log.debug('log file for lws at %s is stored to %s', self._host_name, log_path)

    def _save_lws_core_files(self):
        for remote_core_path in self._installation.list_core_files():
            fname = remote_core_path.name
            artifact_factory = self._artifact_factory(
                ['lws', self._host_name, fname], name=fname, is_error=True, artifact_type=CORE_FILE_ARTIFACT_TYPE)
            local_core_path = artifact_factory.produce_file_path()
            copy_file(remote_core_path, local_core_path)
            log.debug('core file for lws at %s is stored to %s', self._host_name, local_core_path)
            traceback = create_core_file_traceback(
                self._os_access, self._installation.dir / LWS_BINARY_NAME,
                self._physical_installation_host.unpacked_mediaserver_dir / 'lib', remote_core_path)
            artifact_factory = self._artifact_factory(
                ['lws', self._host_name, fname, 'traceback'],
                name='%s-tb' % fname, is_error=True, artifact_type=TRACEBACK_ARTIFACT_TYPE)
            path = artifact_factory.produce_file_path().write_text(traceback)
            log.debug('core file traceback for lws at %s is stored to %s', self._host_name, path)

    def _check_if_server_is_online(self):
        if not self._allocated:
            return
        if self._first_server and self._first_server.is_online() and not self._first_server.is_online():
            log.warning('Lightweight server at %s does not respond to ping - making core dump', self._host_name)
            self._first_server.service.make_core_dump()

    def perform_post_checks(self):
        log.info(
            '----- performing post-test checks for lightweight servers at %s'
            '---------------------->8 ---------------------------',
            self._host_name)
        self._check_if_server_is_online()
        core_file_list = self._installation.list_core_files()
        assert not core_file_list, ('Lightweight server at %s left %d core dump(s): %s' %
                                    (self._host_name, len(core_file_list), ', '.join(core_file_list)))
