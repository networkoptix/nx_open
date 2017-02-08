import os
import os.path
import shutil
import subprocess
from utils import SimpleNamespace
from vagrant_box import Vagrant
from server import (
    MEDIASERVER_DIST_FPATH,
    MEDIASERVER_FORWARDED_PORT_BASE,
    Server,
    )


class BoxConfig(object):

    @classmethod
    def from_dict(cls, d):
        return cls(d['name'], d['forwarded_port'])

    def __init__(self, name, forwarded_port):
        self.name = name
        self.forwarded_port = forwarded_port  # single one for now

    def __repr__(self):
        return 'BoxConfig(%r, %d)' % (self.name, self.forwarded_port)

    def to_dict(self):
        return dict(name=self.name, forwarded_port=self.forwarded_port)


class ServerConfig(object):

    def __init__(self, name, forwarded_port, box_name):
        self.name = name
        self.forwarded_port = forwarded_port
        self.box_name = box_name

    def __repr__(self):
        return 'ServerConfig(%r, %d)' % (self.name, self.forwarded_port)


class ServerFactory(object):

    def __call__(self):
        return None


class EnvironmentBuilder(object):

    vagrant_boxes_cache_key = 'nx/vagrant_boxes'

    def __init__(self, cache, work_dir, reset_server):
        self._work_dir = work_dir
        self._cache = cache
        self._reset_server = reset_server
        self._boxes_config = [BoxConfig.from_dict(d) for d in self._cache.get(self.vagrant_boxes_cache_key, [])]

    def allocate_boxes(self, server_names):
        if self._boxes_config:
            forwarded_port = self._boxes_config[-1].forwarded_port
        else:
            forwarded_port = MEDIASERVER_FORWARDED_PORT_BASE
        servers_config = []
        for i, name in enumerate(server_names):
            if i < len(self._boxes_config):
                box_config = self._boxes_config[i]
            else:
                forwarded_port += 1
                box_config = BoxConfig('funtest-box-%d' % i, forwarded_port)
                self._boxes_config.append(box_config)
            servers_config.append(ServerConfig(name, box_config.forwarded_port, box_config.name))
        self._cache.set(self.vagrant_boxes_cache_key, [config.to_dict() for config in self._boxes_config])
        return servers_config

    def init_server(self, config, boxes):
        box = boxes[config.box_name]
        server = Server(config.name, box, box.ip_address)
        server.wait_until_server_is_up()
        server.store_initial_config()
        server.load_system_settings()
        server.get_storage().cleanup()
        if self._reset_server:
            server.reset()
        else:
            print 'Server reset is skipped'
        server.load_system_settings()
        if not server.is_system_set_up():
            print 'Setting up server %r:' % config.name
            server.setup_local_system()
        return server

    def run_servers_merge(self, server_names, servers):
        if not server_names:
            return
        server_1 = servers[server_names[0]]
        for name in server_names[1:]:
            server_2 = servers[name]
            server_1.merge_systems(server_2)

    def build_environment(self, merge_servers=None, **kw):
        print 'WORK_DIR=%r' % self._work_dir
        vagrant_dir = os.path.join(self._work_dir, 'vagrant')
        if not os.path.isdir(vagrant_dir):
            os.makedirs(vagrant_dir)
        ssh_config_path = os.path.join(self._work_dir, 'ssh.config')
        server_names = sorted(kw.keys())
        servers_config = self.allocate_boxes(server_names)
        print 'servers&boxes config:', servers_config, self._boxes_config
        vagrant = Vagrant(vagrant_dir, ssh_config_path)
        vagrant.create_vagrant_dir()
        shutil.copy2(MEDIASERVER_DIST_FPATH, vagrant_dir)  # distributive is expected at working directory
        vagrant.write_vagrantfile(self._boxes_config)
        vagrant.start_not_started_vagrant_boxes()
        boxes = {box_config.name: vagrant.get_box(box_config) for box_config in self._boxes_config}
        print boxes
        servers = {config.name: self.init_server(config, boxes) for config in servers_config}
        print servers
        self.run_servers_merge(merge_servers or [], servers)
        return SimpleNamespace(**servers)

    def __call__(self, *args, **kw):
        try:
            return self.build_environment(*args, **kw)
        except subprocess.CalledProcessError as x:
            print 'Command %s returned status %d:\n%s' % (x.cmd, x.returncode, x.output)
            raise


def print_list(name, values):
    print '%s:' % name
    for i, value in enumerate(values):
        print '\t #%d: %s' % (i, value)
