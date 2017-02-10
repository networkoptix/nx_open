import os
import os.path
import logging
import shutil
import subprocess
from utils import SimpleNamespace
from vagrant_box import Vagrant
from server_rest_api import REST_API_USER, REST_API_PASSWORD
from server import (
    MEDIASERVER_LISTEN_PORT,
    MEDIASERVER_DIST_FPATH,
    Server,
    )


DEFAULT_HTTP_SCHEMA = 'http'

TEST_DIR = os.path.abspath(os.path.dirname(__file__))

log = logging.getLogger(__name__)


class BoxConfig(object):

    @classmethod
    def from_dict(cls, d):
        return cls(d['name'])

    def __init__(self, name):
        self.name = name

    def __repr__(self):
        return 'BoxConfig(%r)' % self.name

    def to_dict(self):
        return dict(name=self.name)


class ServerConfig(object):

    SETUP_LOCAL = 'local'

    def __init__(self, start=True, setup=SETUP_LOCAL):
        self.start = start
        self.setup = setup
        self.name = None  # set from env_builder kw keys
        self.box_name = None

    def assign_box(self, box_name):
        self.box_name = box_name

    def __repr__(self):
        return 'ServerConfig(%r @ %s)' % (self.name, self.box_name)


class ServerFactory(object):

    def __call__(self, *args, **kw):
        return ServerConfig(*args, **kw)


class EnvironmentBuilder(object):

    vagrant_boxes_cache_key = 'nx/vagrant_boxes'

    def __init__(self, test_session, options, cache, cloud_host_rest_api):
        self._test_session = test_session
        self._cache = cache
        self._work_dir = options.work_dir
        self._bin_dir = options.bin_dir
        self._reset_servers = options.reset_servers
        self._recreate_boxes = options.recreate_boxes
        self._cloud_host_rest_api = cloud_host_rest_api
        self._boxes_config = [BoxConfig.from_dict(d) for d in self._cache.get(self.vagrant_boxes_cache_key, [])]

    def allocate_boxes(self, servers_config):
        for i, config in enumerate(servers_config):
            if i < len(self._boxes_config):
                box_config = self._boxes_config[i]
            else:
                box_config = BoxConfig('funtest-box-%d' % i)
                self._boxes_config.append(box_config)
            config.assign_box(box_config.name)
        self._cache.set(self.vagrant_boxes_cache_key, [config.to_dict() for config in self._boxes_config])

    def init_server(self, http_schema, config, boxes):
        box = boxes[config.box_name]
        url = '%s://%s:%d/' % (http_schema, box.ip_address, MEDIASERVER_LISTEN_PORT)
        server = Server(config.name, box, url, self._cloud_host_rest_api)
        server.init(config.start, self._reset_servers)
        server.storage.cleanup()
        if server.is_started() and not server.is_system_set_up() and config.setup == ServerConfig.SETUP_LOCAL:
            log.info('Setting up server %s:', server)
            server.setup_local_system()
        return server

    def run_servers_merge(self, server_names, servers):
        if not server_names:
            return
        server_1 = servers[server_names[0]]
        assert server_1.is_started(), 'Requested merge of not started server: %s' % server_1
        for name in server_names[1:]:
            server_2 = servers[name]
            assert server_2.is_started(), 'Requested merge of not started server: %s' % server_2
            server_1.merge_systems(server_2)

    def build_environment(self, http_schema=DEFAULT_HTTP_SCHEMA, merge_servers=None,  **kw):
        log.info('WORK_DIR=%r', self._work_dir)
        vagrant_dir = os.path.join(self._work_dir, 'vagrant')
        if not os.path.isdir(vagrant_dir):
            os.makedirs(vagrant_dir)
        ssh_config_path = os.path.join(self._work_dir, 'ssh.config')
        servers_config = []
        for name, value in kw.items():
            if isinstance(value, ServerConfig):
                value.name = name
                servers_config.append(value)
        self.allocate_boxes(servers_config)
        log.info('servers&boxes config: %s %s', servers_config, self._boxes_config)

        dist_fpath = os.path.abspath(os.path.join(self._bin_dir, MEDIASERVER_DIST_FPATH))
        assert os.path.isfile(dist_fpath), '%s is expected at %s' % (os.path.basename(dist_fpath), os.path.dirname(dist_fpath))
        shutil.copy2(dist_fpath, vagrant_dir)  # distributive is expected at working directory

        recreate_boxes = self._recreate_boxes and not self._test_session.boxes_recreated
        vagrant = Vagrant(vagrant_dir, ssh_config_path)
        vagrant.init(self._boxes_config, recreate_boxes)
        if recreate_boxes:
            self._test_session.boxes_recreated = True  # recreate only before first test
        servers = {config.name: self.init_server(http_schema, config, vagrant.boxes) for config in servers_config}
        self.run_servers_merge(merge_servers or [], servers)
        for name, server in servers.items():
            log.info('%s: %r at %s ecs_guid=%r', name.upper(), server.name, server.url, server.ecs_guid)
        log.info('----- setup is complete; test follows ----------------------------->8 ----------------------------------------------')
        return SimpleNamespace(**servers)

    def __call__(self, *args, **kw):
        try:
            return self.build_environment(*args, **kw)
        except subprocess.CalledProcessError as x:
            log.error('Command %s returned status %d:\n%s', x.cmd, x.returncode, x.output)
            raise


def print_list(name, values):
    log.debug('%s:', name)
    for i, value in enumerate(values):
        log.debug('\t #%d: %s', i, value)
