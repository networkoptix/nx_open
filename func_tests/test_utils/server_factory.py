import logging

from pylru import lrudecorator

from test_utils.service import UpstartService
from test_utils.server_installation import install_mediaserver
from .core_file_traceback import create_core_file_traceback
from .server import ServerConfig, Server
from .artifact import ArtifactType

log = logging.getLogger(__name__)

SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')
CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')


class ServerFactory(object):
    def __init__(self,
                 reset_servers,
                 artifact_factory,
                 cloud_host,
                 vagrant_vm_factory,
                 physical_installation_ctl,
                 mediaserver_deb,
                 ca,
                 ):
        self._reset_servers = reset_servers
        self._artifact_factory = artifact_factory
        self._cloud_host = cloud_host
        self._vagrant_vm_factory = vagrant_vm_factory
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
                vm = self._vagrant_vm_factory.get(name)
            installation = install_mediaserver(vm.guest_os_access, self._mediaserver_deb)
            installation.put_key_and_cert(self._ca.generate_key_and_cert())
            vm_host_hostname = vm.host_os_access.hostname
            api_url = '%s://%s:%d/' % (config.http_schema, vm_host_hostname, vm.config.rest_api_forwarded_port)
            customization = self._mediaserver_deb.customization
            service = UpstartService(vm.guest_os_access, customization.service_name)
            if not service.is_running():
                service.set_state(True)
            server = Server(
                config.name, vm.guest_os_access, service, installation, api_url, self._ca, vm,
                rest_api_timeout=config.rest_api_timeout)

        self._allocated_servers.append(server)  # _prepare_server may fail, will need to save it's artifact in that case
        self._prepare_server(config, server)
        log.info('SERVER %s: %s at %s, rest_api=%s ecs_guid=%r local_system_id=%r',
                 server.title, server.name, server.os_access, server.rest_api_url, server.ecs_guid,
                 server.local_system_id)
        return server

    def _prepare_server(self, config, server):
        if config.leave_initial_cloud_host:
            patch_set_cloud_host = None
        else:
            patch_set_cloud_host = self._cloud_host
        server.init(
            must_start=config.start,
            reset=self._reset_servers,
            patch_set_cloud_host=patch_set_cloud_host,
            config_file_params=config.config_file_params,
            )
        if server.is_started() and not server.is_system_set_up() and config.setup:
            if config.setup_cloud_account:
                log.info('Setting up server as local system %s:', server)
                server.setup_cloud_system(config.setup_cloud_account, **config.setup_settings)
            else:
                log.info('Setting up server as local system %s:', server)
                server.setup_local_system(**config.setup_settings)

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
        log_contents = server.get_log_file()
        if not log_contents:
            return
        artifact_factory = self._artifact_factory(
            ['server', server_name], name='server-%s' % server_name, artifact_type=SERVER_LOG_ARTIFACT_TYPE)
        log_path = artifact_factory.produce_file_path().write_bytes(log_contents)
        log.debug('log file for server %s, %s is stored to %s', server.title, server, log_path)

    def _save_server_core_files(self, server):
        server_name = server.title.lower()
        for remote_core_path in server.list_core_files():
            fname = remote_core_path.name
            artifact_factory = self._artifact_factory(
                ['server', server_name, fname],
                name='%s-%s' % (server_name, fname), is_error=True, artifact_type=CORE_FILE_ARTIFACT_TYPE)
            local_core_path = artifact_factory.produce_file_path()
            server.os_access.get_file(remote_core_path, local_core_path)
            log.debug('core file for server %s, %s is stored to %s', server.title, server, local_core_path)
            traceback = create_core_file_traceback(
                server.os_access, server.dir / 'bin/mediaserver-bin', server.dir / 'lib', remote_core_path)
            artifact_factory = self._artifact_factory(
                ['server', server_name, fname, 'traceback'],
                name='%s-%s-tb' % (server_name, fname), is_error=True, artifact_type=TRACEBACK_ARTIFACT_TYPE)
            path = artifact_factory.produce_file_path().write_bytes(traceback)
            log.debug('core file traceback for server %s, %s is stored to %s', server.title, server, path)

    def _check_if_servers_are_online(self):
        for server in self._allocated_servers:
            if server.is_started() and not server.is_server_online():
                log.warning('Server %s is started but does not respond to ping - making core dump', server)
                server.make_core_dump()

    def perform_post_checks(self):
        log.info('Perform post-test checks.')
        self._check_if_servers_are_online()
        core_dumped_servers = []
        for server in self._allocated_servers:
            if server.list_core_files():
                core_dumped_servers.append(server.title)
        assert not core_dumped_servers, 'Following server(s) left core dump(s): %s' % ', '.join(core_dumped_servers)
