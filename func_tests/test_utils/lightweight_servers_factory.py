# create lightweight servers (LWS) containers implemented by appserver2_ut with requested number of servers in them

import os.path
import logging
import time
from .template_renderer import TemplateRenderer
from . import utils
from .server_ctl import SERVER_CTL_TARGET_PATH, PhysicalHostServerCtl
from .server import Server

log = logging.getLogger(__name__)


LWS_CTL_TEMPLATE_PATH = 'lws_ctl.sh.jinja2'
LWS_PORT_BASE = 3000


class LightweightServersFactory(object):

    test_binary_name = 'appserver2_ut'

    def __init__(self, artifact_factory, physical_installation_ctl, test_binary_path):
        self._artifact_factory = artifact_factory
        self._physical_installation_ctl = physical_installation_ctl
        self._test_binary_path = test_binary_path
        self._physical_installation_host = self._pick_server()
        self._lws_host = LightweightServersHost(self._artifact_factory, self._test_binary_path, self._physical_installation_host)

    def __call__(self, server_count):
        return self._allocate(server_count)

    def release(self):
        self._lws_host.release()

    def perform_post_checks(self):
        self._lws_host.perform_post_checks()

    def _pick_server(self):
        for config, host in zip(self._physical_installation_ctl.config_list,
                                self._physical_installation_ctl.installation_hosts):
            if config.lightweight_servers_limit:
                log.info('Lightweight host: %s %s', config, host)
                return host
        assert False, 'No lightweight servers configured'

    def _allocate(self, server_count):
        return list(self._lws_host.allocate(server_count))


class LightweightServersInstallation(object):

    def __init__(self, host, dir):
        self.host = host
        self.dir = dir
        self.log_path_base = os.path.join(dir, 'lws')

    def cleanup_var_dir(self):
        self._not_supported()

    def list_core_files(self):
        return self.host.expand_glob(os.path.join(self.dir, '*core*'))

    def cleanup_core_files(self):
        for path in self.list_core_files():
            self.host.run_command(['rm', path])

    def reset_config(self, **kw):
        self._not_supported()

    def change_config(self, **kw):
        self._not_supported()

    def get_log_file(self):
        if self.host.file_exists(self._log_path):
            return self.host.read_file(self.log_path_base + '.log')
        else:
            return None

    def _not_supported(self):
        assert False, 'This operation is not supported by lightweight server'


class LightweightServer(Server):

    def __init__(self, name, host, installation, server_ctl, rest_api_url, internal_ip_port=None, timezone=None):
        Server.__init__(self, name, host, installation, server_ctl, rest_api_url,
                            internal_ip_port=internal_ip_port, timezone=timezone)
        self.internal_ip_address = host.host
        self._state = self._st_started

    def load_system_settings(self):
        response = self.rest_api.api.ping.GET()
        self.local_system_id = response['localSystemId']

    def wait_until_synced(self, timeout):
        log.info('Waiting for lightweight servers to merge between themselves')
        start_time = utils.datetime_utc_now()
        while utils.datetime_utc_now() - start_time < timeout:
            response = self.rest_api.api.moduleInformation.GET()
            if response['serverFlags'] == 'SF_P2pSyncDone':
                log.info('Lightweight servers merged between themselves in %s' % (utils.datetime_utc_now() - start_time))
                return
            time.sleep(10)
        assert False, 'Lightweight servers did not merge between themselves in %s' % timeout


class LightweightServersHost(object):

    def __init__(self, artifact_factory, test_binary_path, physical_installation_host):
        self._artifact_factory = artifact_factory
        self._test_binary_path = test_binary_path
        self._physical_installation_host = physical_installation_host
        self._host = physical_installation_host.host
        self._host_name = physical_installation_host.name
        self._timezone = physical_installation_host.timezone
        self._installation = LightweightServersInstallation(
            self._host, os.path.join(physical_installation_host.root_dir, 'lws'))
        self._template_renderer = TemplateRenderer()
        self._allocated = False

    # server_count is ignored for now; all servers are allocated on first lightweight installation
    def allocate(self, server_count):
        assert not self._allocated, 'Lightweight servers were already allocated by this test'
        pih = self._physical_installation_host
        pih.ensure_mediaserver_is_unpacked()
        server_dir = pih.unpacked_mediaserver_dir
        lws_dir = self._installation.dir
        self._host.mk_dir(lws_dir)
        server_ctl = PhysicalHostServerCtl(self._host, lws_dir)
        if server_ctl.get_state():
            server_ctl.set_state(is_started=False)
        self._cleanup_core_files()
        self._cleanup_log_files()
        self._host.put_file(self._test_binary_path, lws_dir)
        self._write_lws_ctl(server_dir, lws_dir, server_count)
        server_ctl.set_state(is_started=True)
        for idx in range(server_count):
            server_port = LWS_PORT_BASE + idx
            rest_api_url = '%s://%s:%d/' % ('http', self._host.host, server_port)
            server = LightweightServer('lws-%05d' % idx, self._host, self._installation, server_ctl, rest_api_url,
                                       internal_ip_port=server_port, timezone=self._timezone)
            response = server.wait_for_server_become_online(check_interval_sec=2)
            server.local_system_id = response['localSystemId']
            yield server
        self._allocated = True

    def release(self):
        if self._allocated:
            self._save_lws_artifacts()
        self._allocated = False

    def _cleanup_core_files(self):
        for path in self._installation.list_core_files():
            self._host.run_command(['rm', path])

    def _cleanup_log_files(self):
        file_list = self._host.expand_glob(os.path.join(self._installation.dir, 'lws*.log'))
        if file_list:
            self._host.run_command(['rm'] + file_list)

    def _write_lws_ctl(self, server_dist_dir, lws_dir, server_count):
        contents = self._template_renderer.render(
            LWS_CTL_TEMPLATE_PATH,
            SERVER_DIR=lws_dir,
            MEDIASERVER_DIST_DIR=server_dist_dir,
            LOG_PATH_BASE=self._installation.log_path_base,
            SERVER_COUNT=server_count,
            PORT_BASE=LWS_PORT_BASE,
            )
        lws_ctl_path = os.path.join(lws_dir, SERVER_CTL_TARGET_PATH)
        self._host.write_file(lws_ctl_path, contents)
        self._host.run_command(['chmod', '+x', lws_ctl_path])

    def _save_lws_artifacts(self):
        self._save_lws_log()
        self._save_core_files()

    def _save_lws_log(self):
        log_contents = self._host.read_file(self._installation.log_path_base + '.log').strip()
        if log_contents:
            artifact_factory = self._artifact_factory(['lws', self._host_name], ext='.log', name='lws', type_name='log')
            log_path = artifact_factory.produce_file_path()
            with open(log_path, 'wb') as f:
                f.write(log_contents)
            log.debug('log file for lws at %s is stored to %s', self._host_name, log_path)

    def _save_core_files(self):
        for remote_core_path in self._installation.list_core_files():
            fname = os.path.basename(remote_core_path)
            artifact_factory = self._artifact_factory(['lws', self._host_name, fname], name=fname, is_error=True, type_name='core')
            local_core_path = artifact_factory.produce_file_path()
            self._host.get_file(remote_core_path, local_core_path)
            log.debug('core file for lws at %s is stored to %s', self._host_name, local_core_path)

    def perform_post_checks(self):
        log.info('----- performing post-test checks for lightweight servers at %s'
                     '---------------------->8 ---------------------------', self._host_name)
        core_file_list = self._installation.list_core_files()
        assert not core_file_list, ('Lightweight server at %s left %d core dump(s): %s' %
                                        (self._host_name, len(core_file_list), ', '.join(core_file_list)))
