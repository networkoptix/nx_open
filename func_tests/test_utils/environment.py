'''Main utilities module for functional tests

Used to create test environmant - virtual boxes, servers.
'''

import os
import os.path
import logging
import shutil
import subprocess
from .host import host_from_config
from .vagrant_box_config import BoxConfig
from .vagrant_box import Vagrant
from .server_rest_api import REST_API_USER, REST_API_PASSWORD
from .server import (
    MEDIASERVER_LISTEN_PORT,
    ServerConfig,
    Server,
    )


DEFAULT_HTTP_SCHEMA = 'http'

log = logging.getLogger(__name__)

 
class EnvironmentBuilder(object):

    vagrant_boxes_cache_key = 'nx/vagrant_boxes'

    def __init__(self, request, test_session, options, cache, cloud_host_host, company_name):
        self._request = request
        self._test_id = request.node.nodeid
        self._test_dir = os.path.abspath(os.path.dirname(str(request.node.fspath)))
        self._test_session = test_session
        self._cache = cache
        self._cloud_host_host = cloud_host_host  # cloud host dns name, like: 'cloud-dev.hdw.mx'
        self._company_name = company_name
        self._work_dir = options.work_dir
        self._bin_dir = options.bin_dir
        self._reset_servers = options.reset_servers
        self._recreate_boxes = options.recreate_boxes
        self._vm_name_prefix = options.vm_name_prefix
        self._vm_is_local_host = options.vm_ssh_host_config is None
        self._vm_host = host_from_config(options.vm_ssh_host_config)
        self._vm_host_work_dir = options.vm_host_work_dir
        self._boxes_config = []
        self._last_box_idx = 0

    def _load_boxes_config_from_cache(self):
        return [BoxConfig.from_dict(d, self._vm_name_prefix)
                for d in self._cache.get(self.vagrant_boxes_cache_key, [])]

    def _save_boxes_config_to_cache(self):
        self._cache.set(self.vagrant_boxes_cache_key, [config.to_dict() for config in self._boxes_config])

    def _allocate_boxes(self, servers_config, boxes_config):
        assigned_configs = {server.box for server in servers_config}  # must be created with original server configs
        # first allocate boxes assigned to servers
        for server_config in servers_config:
            config = self._allocate_box_config(server_config.box)
            server_config.box = config
            if server_config.leave_initial_cloud_host:
                config.must_be_recreated = True  # to have initial cloud host we need binaries unchanged/fresh
        # now allocate boxes not assigned to servers
        for required_config in boxes_config:
            if required_config not in assigned_configs:
                unused_config = self._allocate_box_config(required_config)

    def _allocate_box_config(self, required_config):
        config = self._find_matching_box_config(required_config)
        if config:
            config.name = required_config.name
        else:
            config = required_config
            self._last_box_idx += 1
            config.idx = self._last_box_idx
            config.vm_name_prefix = self._vm_name_prefix
            self._boxes_config.append(config)
        config.is_allocated = True
        log.info('BOX CONFIG %s: %s', config.box_name(), config)
        return config

    def _find_matching_box_config(self, required_config):
        for config in self._boxes_config:
            if config.is_allocated:
                continue
            if config.matches(required_config):
                return config
        return None

    def _init_server(self, box_config_to_box, http_schema, config):
        box = box_config_to_box[config.box]
        url = '%s://%s:%d/' % (http_schema, box.ip_address, MEDIASERVER_LISTEN_PORT)
        server = Server(self._company_name, config.name, box, url)
        if config.leave_initial_cloud_host:
            patch_set_cloud_host = None
        else:
            patch_set_cloud_host = self._cloud_host_host
        server.init(config.start, self._reset_servers, patch_set_cloud_host=patch_set_cloud_host,
                    config_file_params=config.config_file_params)
        if server.is_started() and not server.is_system_set_up() and config.setup == ServerConfig.SETUP_LOCAL:
            log.info('Setting up server %s:', server)
            server.setup_local_system()
        return server

    def _run_servers_merge(self, servers_config, servers):
        if not servers_config:
            return
        server_1 = servers[servers_config[0].name]
        assert server_1.is_started(), 'Requested merge of not started server: %s' % server_1
        for config in servers_config[1:]:
            server_2 = servers[config.name]
            assert server_2.is_started(), 'Requested merge of not started server: %s' % server_2
            server_1.merge_systems(server_2)

    def build_environment(self, http_schema=DEFAULT_HTTP_SCHEMA, boxes=None, merge_servers=None,  **kw):
        if not boxes:
            boxes = []
        log.info('TEST_DIR=%r, WORK_DIR=%r, BIN_DIR=%r, CLOUD_HOST=%r, COMPANY_NAME=%r',
                 self._test_dir, self._work_dir, self._bin_dir, self._cloud_host_host, self._company_name)
        self._boxes_config = self._load_boxes_config_from_cache()
        ssh_config_path = os.path.join(self._work_dir, 'ssh.config')

        if self._vm_is_local_host:
            vagrant_dir = os.path.join(self._work_dir, 'vagrant')
        else:
            vagrant_dir = os.path.join(self._vm_host_work_dir, 'vagrant')
        vagrant = Vagrant(self._test_dir, self._vm_host, self._bin_dir, vagrant_dir,
                          self._test_session.vagrant_private_key_path, ssh_config_path)

        if self._test_session.must_recreate_boxes():
            # to be able to destroy all old boxes we need old boxes config to create Vagrantfile with them
            vagrant.destroy_all_boxes(self._boxes_config)
            self._boxes_config = []  # now we can start afresh
            self._test_session.set_boxes_recreated_flag()
        
        if self._boxes_config:
            self._last_box_idx = max(box.idx for box in self._boxes_config)
        
        servers_config = []
        for name, value in kw.items():
            if isinstance(value, ServerConfig):
                value.name = name
                servers_config.append(value)
                if value.box not in boxes:
                    boxes.append(value.box)

        self._allocate_boxes(servers_config, boxes)

        try:
            vagrant.init(self._boxes_config)
            box_config_to_box = {box.config: box for box in vagrant.boxes.values()}
        finally:
            self._save_boxes_config_to_cache()  # vagrant.init may change config, must save it after this

        for box in vagrant.boxes.values():
            log.info('BOX %s', box)

        servers = {}
        for config in servers_config:
            servers[config.name] = self._init_server(box_config_to_box, http_schema, config)
        self._run_servers_merge(merge_servers or [], servers)

        for name, server in servers.items():
            log.info('SERVER %s: %r at %s %s ecs_guid=%r local_system_id=%r',
                     name.upper(), server.name, server.box.name, server.url, server.ecs_guid, server.local_system_id)
        log.info('----- build environment setup is complete ----------------------------->8 ----------------------------------------------')
        artifact_path_prefix = os.path.join(
            self._work_dir,
            os.path.basename(self._test_id.replace(':', '_').replace('.py', '')))
        return Environment(artifact_path_prefix, servers)

    def __call__(self, *args, **kw):
        try:
            env = self.build_environment(*args, **kw)
            self._request.addfinalizer(env.finalizer)
            return env
        except subprocess.CalledProcessError as x:
            log.error('Command %s returned status %d:\n%s', x.cmd, x.returncode, x.output)
            raise


class Environment(object):

    def __init__(self, artifact_path_prefix, servers):
        self.artifact_path_prefix = artifact_path_prefix
        self.servers = servers
        for name, server in servers.items():
            setattr(self, name, server)

    def finalizer(self):
        log.info('FINALIZER for %s', self.artifact_path_prefix)
        for name, server in self.servers.items():
            log_path = '%s-server-%s.log' % (self.artifact_path_prefix, name)
            with open(log_path, 'wb') as f:
                f.write(server.get_log_file())
            log.debug('log file for server %s, %s is stored to %s', name.upper(), server, log_path)
