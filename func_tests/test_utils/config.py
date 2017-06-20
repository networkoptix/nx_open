import os.path
import argparse
import yaml
import re
from .utils import SimpleNamespace
from .server_physical_host import PhysicalInstallationHostConfig
from datetime import timedelta
from .host import SshHostConfig


def expand_path(path):
    path = os.path.expandvars(path)
    path = os.path.expanduser(path)
    path = os.path.abspath(path)
    return path

TIMEDELTA_REGEXP = re.compile(r'^((?P<days>\d+?)d)?((?P<hours>\d+?)h)?((?P<minutes>\d+?)m)?((?P<seconds>\d+?)s)?$')

def timedelta_constructor(loader, node):
    value = loader.construct_scalar(node)
    match = TIMEDELTA_REGEXP.match(value)
    try:
        if not match: return timedelta(seconds=int(duration_str))
        timedelta_params = {k: int(v)
                            for (k, v) in match.groupdict().iteritems() if v}
        if not timedelta_params:
            return timedelta(seconds=int(duration_str))
        return timedelta(**timedelta_params)
    except ValueError:
        return None


def timedelta_representer(dumper, data):
    assert isinstance(data, timedelta)
    hours = data.seconds / (60 * 60)
    minutes = (data.seconds - hours * 60 * 60) / 60
    seconds = data.seconds % 60
    return dumper.represent_scalar(u'!timedelta', u'%dd%dh%dm%ds' % (
        data.days, hours, minutes, seconds))


yaml.add_constructor(u'!timedelta', timedelta_constructor)
yaml.add_representer(timedelta, timedelta_representer)
yaml.add_implicit_resolver(u'!timedelta', TIMEDELTA_REGEXP)


class TestsConfig(object):

    @classmethod
    def from_yaml_file(cls, file_path):
        full_path = expand_path(file_path)
        if not os.path.isfile(full_path):
            raise argparse.ArgumentTypeError('file does not exist: %s' % full_path)
        with open(full_path) as f:
            d = yaml.load(f)
        return cls.from_dict(d)

    @classmethod
    def from_dict(cls, d):
        installations = map(PhysicalInstallationHostConfig.from_dict, d.get('hosts') or [])
        tests = d.get('tests')
        return cls(installations, tests)

    @staticmethod
    def merge_config_list(config_list):
        if not config_list:
            return None
        config = config_list[0]
        for other in config_list[1:]:
            config.update(other)
        return config

    def __init__(self, physical_installation_host_list, tests=None):
        self.physical_installation_host_list = physical_installation_host_list  # PhysicalInstallationHostConfig list
        self.tests = tests or {}  # dict, full test name -> dict

    def update(self, other):
        assert isinstance(other, TestsConfig), repr(other)
        self.physical_installation_host_list += other.physical_installation_host_list

    def get_test_config(self, full_node_id):
        module_path, test_name = full_node_id.split('::')
        module_name = os.path.splitext(os.path.basename(module_path))[0]
        node_id = module_name + '::' + test_name
        while node_id:
            config = self.tests.get(node_id)
            if config:
                return SingleTestConfig(config)
            node_id = self._parent_node_id(node_id)
        return SingleTestConfig()  # no config in file - all test defaults will be used then

    def _parent_node_id(self, node_id):
        if '[' in node_id:
            return node_id.split('[')[0]
        if '::' in node_id:
            return node_id.split('::')[0]
        if '/' in node_id:
            return node_id.rsplit('/', 1)[0]
        return None


class SingleTestConfig(object):

    def __init__(self, config=None):
        self._config = config or {}  # dict

    def with_defaults(self, **kw):
        return SimpleNamespace(
            **{name: self._config.get(name.lower(), default_value)
               for name, default_value in kw.items()})
