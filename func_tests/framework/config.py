import argparse
import datetime
import logging
import re

import six
import yaml
from pathlib2 import Path

from .utils import SimpleNamespace

_logger = logging.getLogger(__name__)


TIMEDELTA_REGEXP = re.compile(r'^((?P<days>\d+?)d)?((?P<hours>\d+?)h)?((?P<minutes>\d+?)m)?((?P<seconds>\d+?)s)?$')


def timedelta_constructor(loader, node):
    value = loader.construct_scalar(node)
    return str_to_timedelta(value)


def str_to_timedelta(duration_str):
    match = TIMEDELTA_REGEXP.match(duration_str)
    try:
        if not match:
            return datetime.timedelta(seconds=int(duration_str))
        timedelta_params = {k: int(v) for (k, v) in match.groupdict().items() if v}
        if not timedelta_params:
            return datetime.timedelta(seconds=int(duration_str))
        return datetime.timedelta(**timedelta_params)
    except ValueError:
        return None


def timedelta_representer(dumper, data):
    assert isinstance(data, datetime.timedelta)
    hours = data.seconds / (60 * 60)
    minutes = (data.seconds - hours * 60 * 60) / 60
    seconds = data.seconds % 60
    return dumper.represent_scalar(u'!timedelta', u'%dd%dh%dm%ds' % (
        data.days, hours, minutes, seconds))


yaml.add_constructor(u'!timedelta', timedelta_constructor)
yaml.add_representer(datetime.timedelta, timedelta_representer)
yaml.add_implicit_resolver(u'!timedelta', TIMEDELTA_REGEXP)


class TestParameter(object):

    @classmethod
    def from_str(cls, str_list):
        parameters = []
        for s in str_list.split(','):
            name_value = s.split('=')
            test_param = name_value[0].split('.')
            if len(name_value) != 2 or len(test_param) != 2:
                raise argparse.ArgumentTypeError('test-parameter is expected in format "test.param=value": %s' % s)
            parameters.append(cls(test_param[0], test_param[1], name_value[1]))
        return parameters

    def __init__(self, test_id, parameter, value):
        self.test_id = test_id
        self.parameter = parameter
        self.value = value


class TestsConfig(object):

    @classmethod
    def from_yaml_file(cls, file_path):
        full_path = Path(file_path)  # Accepts option value.
        if not full_path.exists():
            raise argparse.ArgumentTypeError('file does not exist: %s' % full_path)
        with full_path.open() as f:
            d = yaml.load(f)
        return cls.from_dict(d)

    @classmethod
    def from_dict(cls, d):
        tests = d.get('tests')
        return cls(tests)

    @classmethod
    def merge_config_list(cls, config_list, test_params):
        if config_list:
            config = config_list[0]
            for other in config_list[1:]:
                config.update(other)
        elif test_params:
            config = cls()
        else:
            return None
        if test_params:
            config._update_with_tests_params(test_params)
        return config

    def __init__(self, tests=None):
        self.tests = tests or {}  # {}, full test name -> {}

    def _update_with_tests_params(self, test_params):
        for p in test_params:
            self.tests.setdefault(p.test_id, {})[p.parameter] = p.value

    def update(self, other):
        assert isinstance(other, TestsConfig), repr(other)
        self.tests.update(other.tests)

    def get_test_config(self, full_node_id):
        module_path_as_str, test_name = full_node_id.split('::')
        module_name = Path(module_path_as_str).stem
        node_id = module_name + '::' + test_name
        while node_id:
            config = self.tests.get(node_id)
            if config:
                return SingleTestConfig(config)
            node_id = self._parent_node_id(node_id)
        return SingleTestConfig()  # no config in file - all test defaults will be used then

    @staticmethod
    def _parent_node_id(node_id):
        if '[' in node_id:
            return node_id.split('[')[0]
        if '::' in node_id:
            return node_id.split('::')[0]
        if '/' in node_id:
            return node_id.rsplit('/', 1)[0]
        return None


class SingleTestConfig(object):

    def __init__(self, config=None):
        self._config = config or {}  # {}

    # parameters in config file and parameters which do not have defaults in test are ignored
    def with_defaults(self, **kw):
        config = {}
        for name, default_value in kw.items():
            value = self._config.get(name.lower())
            if value is not None:
                config[name] = self._cast_value(value, type(default_value))
            else:
                config[name] = default_value
            _logger.info('Test config: %s = %r', name, config[name])
        return SimpleNamespace(**config)

    @staticmethod
    def _cast_value(value, t):
        if not isinstance(value, six.string_types):
            return value
        if t is int:
            return int(value)
        if t is bool:
            if value.lower() in ['true', 'yes', 'on']:
                return True
            if value.lower() in ['false', 'no', 'off']:
                return False
            assert False, 'Invalid bool value: %r; Accepted values are: true, false, yes, no, on, off' % value.lower()
        if t is datetime.timedelta:
            return str_to_timedelta(value)
        return value
