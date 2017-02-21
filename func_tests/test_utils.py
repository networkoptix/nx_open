import os
import os.path
import logging
import shutil
import subprocess
from utils import SimpleNamespace
from vagrant_box_config import box_config_factory, BoxConfig
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

 
class ServerConfig(object):

    SETUP_LOCAL = 'local'

    def __init__(self, start=True, setup=SETUP_LOCAL, box=None):
        assert box is None or isinstance(box, BoxConfig), repr(box)
        self.start = start
        self.setup = setup
        self.name = None  # set from env_builder kw keys
        self.box = box or box_config_factory()

    def __repr__(self):
        return 'ServerConfig(%r @ %s)' % (self.name, self.box)


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
        self._boxes_config = []
        self._last_box_idx = 0

    def _load_boxes_config_from_cache(self):
        return [BoxConfig.from_dict(d) for d in self._cache.get(self.vagrant_boxes_cache_key, [])]

    def _save_boxes_config_to_cache(self):
        self._cache.set(self.vagrant_boxes_cache_key, [config.to_dict() for config in self._boxes_config])

    def _allocate_boxes(self, boxes_config):
        for required_config in boxes_config:
            config = self._find_matching_box_config(required_config)
            if config:
                config.name = required_config.name
            else:
                config = required_config
                self._last_box_idx += 1
                config.idx = self._last_box_idx
                self._boxes_config.append(config)
            log.info('BOX CONFIG %s: %s', config.box_name(), config)
            config.is_allocated = True

    def _find_matching_box_config(self, required_config):
        for config in self._boxes_config:
            if config.is_allocated:
                continue
            if config.matches(required_config):
                return config
        return None

    def _find_matching_box(self, vagrant, used_boxes, required_config):
        for box in vagrant.boxes.values():
            if box in used_boxes:
                continue
            if box.config.matches(required_config):
                used_boxes.add(box)
                return box
        assert False  # bug in this module, report to developer

    def _init_server(self, vagrant, used_boxes, http_schema, config):
        box = self._find_matching_box(vagrant, used_boxes, config.box)
        url = '%s://%s:%d/' % (http_schema, box.ip_address, MEDIASERVER_LISTEN_PORT)
        server = Server(config.name, box, url, self._cloud_host_rest_api)
        server.init(config.start, self._reset_servers)
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
        recreate_boxes = self._recreate_boxes and not self._test_session.boxes_recreated
        if not boxes:
            boxes = []
        log.info('WORK_DIR=%r', self._work_dir)
        self._boxes_config = self._load_boxes_config_from_cache()

        vagrant_dir = os.path.join(self._work_dir, 'vagrant')
        if not os.path.isdir(vagrant_dir):
            os.makedirs(vagrant_dir)
        ssh_config_path = os.path.join(self._work_dir, 'ssh.config')

        vagrant = Vagrant(vagrant_dir, ssh_config_path)
        if recreate_boxes:
            # to be able to destroy all old boxes we need old boxes config to create Vagrantfile with them
            vagrant.destroy_all_boxes(self._boxes_config)
            self._boxes_config = []  # now we can start afresh
            self._test_session.boxes_recreated = True  # recreate only before first test
        
        if self._boxes_config:
            self._last_box_idx = max(box.idx for box in self._boxes_config)
        
        servers_config = []
        for name, value in kw.items():
            if isinstance(value, ServerConfig):
                value.name = name
                servers_config.append(value)
                if value.box not in boxes:
                    boxes.append(value.box)
        self._allocate_boxes(boxes)

        dist_fpath = os.path.abspath(os.path.join(self._bin_dir, MEDIASERVER_DIST_FPATH))
        assert os.path.isfile(dist_fpath), \
          '%s is expected at %s' % (os.path.basename(dist_fpath), os.path.dirname(dist_fpath))
        shutil.copy2(dist_fpath, vagrant_dir)  # distributive is expected at working directory

        vagrant.init(self._boxes_config)
        self._save_boxes_config_to_cache()
        for box in vagrant.boxes.values():
            log.info('BOX %s', box)

        used_boxes = set()
        servers = {config.name: self._init_server(vagrant, used_boxes, http_schema, config)
                   for config in servers_config}
        self._run_servers_merge(merge_servers or [], servers)
        for name, server in servers.items():
            log.info('SERVER %s: %r at %s %s ecs_guid=%r local_system_id=%r',
                     name.upper(), server.name, server.box.name, server.url, server.ecs_guid, server.local_system_id)
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

def bool_to_str(val, falseStr = 'false', trueStr = 'true'):
    if val: return trueStr
    else: return falseStr

def str_to_bool(val):
    v = val.lower()
    if val == 'false' or val == '0':
        return False
    elif val == 'true' or val == '1':
        return True
    raise Exception('Invalid boolean "%s"' % val)
