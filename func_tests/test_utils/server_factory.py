import logging

from pylru import lrudecorator

from test_utils.rest_api import RestApi
from test_utils.server import DEFAULT_SERVER_LOG_LEVEL
from test_utils.server_installation import install_mediaserver
from test_utils.service import UpstartService
from .artifact import ArtifactType
from .core_file_traceback import create_core_file_traceback
from .server import Server, ServerConfig

log = logging.getLogger(__name__)

SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')
CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')


class ServerFactory(object):
    def __init__(self, artifact_factory, cloud_host, vm_pool, physical_installation_ctl, mediaserver_deb, ca):
        self._artifact_factory = artifact_factory
        self._cloud_host = cloud_host
        self._linux_vm_pool = vm_pool
        self._physical_installation_ctl = physical_installation_ctl  # PhysicalInstallationCtl or None
        self._allocated_servers = []
        self._ca = ca
        self._mediaserver_deb = mediaserver_deb

    @lrudecorator(1000)
    def get(self, name):
        return self.create(name)

    def create(self, name, *args, **kw):
        config = ServerConfig(name, *args, **kw)
        server = None
        if self._physical_installation_ctl and not config.vm:  # this server requires specific vm, can not allocate it on physical one
            server = self._physical_installation_ctl.allocate_server(config)
        if not server:
            if config.vm:
                vm = config.vm
            else:
                vm = self._linux_vm_pool.get(name)
            hostname, port = vm.ports['tcp', 7001]
            api_url = '%s://%s:%d/' % (config.http_schema, hostname, port)
            server = Server(
                name,
                UpstartService(vm.os_access, self._mediaserver_deb.customization.service),
                install_mediaserver(vm.os_access, self._mediaserver_deb),
                RestApi(name, api_url, timeout=config.rest_api_timeout),
                vm)
        self._allocated_servers.append(server)  # Following may fail, will need to save it's artifact in that case.
        server.stop(already_stopped_ok=True)
        server.installation.cleanup_core_files()
        server.installation.cleanup_var_dir()
        server.installation.reset_config(
            logLevel=DEFAULT_SERVER_LOG_LEVEL,
            tranLogLevel=DEFAULT_SERVER_LOG_LEVEL,
            **(config.config_file_params or {}))
        server.installation.put_key_and_cert(self._ca.generate_key_and_cert())
        if not config.leave_initial_cloud_host:
            server.patch_binary_set_cloud_host(self._cloud_host)  # may be changed by previous tests...
        server.start()
        if config.setup:
            if config.setup_cloud_account:
                server.setup_cloud_system(config.setup_cloud_account, **config.setup_settings)
            else:
                server.setup_local_system(**config.setup_settings)
        return server

    def release(self):
        self.get.clear()
        self._check_if_servers_are_online()
        for server in self._allocated_servers:
            self._save_server_artifacts(server)
        if self._physical_installation_ctl:
            self._physical_installation_ctl.release_all_servers()
        self._allocated_servers = []

    def _save_server_artifacts(self, server):
        self._save_server_log(server)
        self._save_server_core_files(server)

    def _save_server_log(self, server):
        server_name = server.title.lower()
        log_contents = server.installation.get_log_file()
        if not log_contents:
            return
        artifact_factory = self._artifact_factory(
            ['server', server_name], name='server-%s' % server_name, artifact_type=SERVER_LOG_ARTIFACT_TYPE)
        log_path = artifact_factory.produce_file_path().write_bytes(log_contents)
        log.debug('log file for server %s, %s is stored to %s', server.title, server, log_path)

    def _save_server_core_files(self, server):
        server_name = server.title.lower()
        for remote_core_path in server.installation.list_core_files():
            fname = remote_core_path.name
            artifact_factory = self._artifact_factory(
                ['server', server_name, fname],
                name='%s-%s' % (server_name, fname), is_error=True, artifact_type=CORE_FILE_ARTIFACT_TYPE)
            local_core_path = artifact_factory.produce_file_path()
            server.os_access.get_file(remote_core_path, local_core_path)
            log.debug('core file for server %s, %s is stored to %s', server.title, server, local_core_path)
            traceback = create_core_file_traceback(
                server.os_access, server.installation.dir / 'bin/mediaserver-bin', server.dir / 'lib', remote_core_path)
            artifact_factory = self._artifact_factory(
                ['server', server_name, fname, 'traceback'],
                name='%s-%s-tb' % (server_name, fname), is_error=True, artifact_type=TRACEBACK_ARTIFACT_TYPE)
            path = artifact_factory.produce_file_path().write_bytes(traceback)
            log.debug('core file traceback for server %s, %s is stored to %s', server.title, server, path)

    def _check_if_servers_are_online(self):
        for server in self._allocated_servers:
            if server.service.is_running() and not server.is_online():
                log.warning('Server %s is started but does not respond to ping - making core dump', server)
                server.service.make_core_dump()

    def perform_post_checks(self):
        log.info('Perform post-test checks.')
        self._check_if_servers_are_online()
        core_dumped_servers = []
        for server in self._allocated_servers:
            if server.installation.list_core_files():
                core_dumped_servers.append(server.title)
        assert not core_dumped_servers, 'Following server(s) left core dump(s): %s' % ', '.join(core_dumped_servers)
