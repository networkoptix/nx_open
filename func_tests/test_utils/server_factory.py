import os.path
import logging
from .server import ServerConfig

log = logging.getLogger(__name__)


class ServerFactory(object):

    def __init__(self,
                 reset_servers,
                 artifact_factory,
                 customization_company_name,
                 cloud_host_host,
                 vagrant_box_factory,
                 physical_installation_ctl,
                 ):
        self._reset_servers = reset_servers
        self._artifact_factory = artifact_factory
        self._customization_company_name = customization_company_name
        self._cloud_host_host = cloud_host_host
        self._vagrant_box_factory = vagrant_box_factory
        self._physical_installation_ctl = physical_installation_ctl  # PhysicalInstallationCtl or None
        self._allocated_servers = []

    def __call__(self, *args, **kw):
        config = ServerConfig(*args, **kw)
        return self._allocate(config)

    def _allocate(self, config):
        server = None
        if self._physical_installation_ctl and not config.box:  # this server requires specific vm box, can not allocate it on physical one
            server = self._physical_installation_ctl.allocate_server(config)
        if not server:
            if config.box:
                box = config.box
            else:
                box = self._vagrant_box_factory(must_be_recreated=config.leave_initial_cloud_host)
            server = box.get_server(config)
        self._prepare_server(config, server)
        self._allocated_servers.append(server)
        log.info('SERVER %s: %s at %s, rest_api=%s ecs_guid=%r local_system_id=%r',
                 server.title, server.name, server.host, server.rest_api_url, server.ecs_guid, server.local_system_id)
        return server

    def _prepare_server(self, config, server):
        if config.leave_initial_cloud_host:
            patch_set_cloud_host = None
        else:
            patch_set_cloud_host = self._cloud_host_host
        server.init(
            must_start=config.start,
            reset=self._reset_servers,
            patch_set_cloud_host=patch_set_cloud_host,
            config_file_params=config.config_file_params,
            )
        if server.is_started() and not server.is_system_set_up() and config.setup:
            if config.setup_cloud_host:
                log.info('Setting up server as local system %s:', server)
                server.setup_cloud_system(config.setup_cloud_host, **config.setup_settings)
            else:
                log.info('Setting up server as local system %s:', server)
                server.setup_local_system(**config.setup_settings)

    def release(self):
        for server in self._allocated_servers:
            self._save_server_artifacts(server)
        if self._physical_installation_ctl:
            self._physical_installation_ctl.release_all()
        self._allocated_servers = []

    def _save_server_artifacts(self, server):
        log_contents = server.get_log_file()
        if log_contents:
            log_path = self._artifact_factory('server', server.title.lower(), '.log')
            with open(log_path, 'wb') as f:
                f.write(log_contents)
            log.debug('log file for server %s, %s is stored to %s', server.title, server, log_path)
        for remote_core_path in server.list_core_files():
            local_core_path = self._artifact_factory('server', server.title.lower(), os.path.basename(remote_core_path))
            server.host.get_file(remote_core_path, local_core_path)
            log.debug('core file for server %s, %s is stored to %s', server.title, server, local_core_path)

    def perform_post_checks(self):
        log.info('----- test is finished, performing post-test checks ------------------------>8 -----------------------------------------')
        core_dumped_servers = []
        for server in self._allocated_servers:
            if server.list_core_files():
                core_dumped_servers.append(server.title)
        assert not core_dumped_servers, 'Following server(s) left core dump(s): %s' % ', '.join(core_dumped_servers)
