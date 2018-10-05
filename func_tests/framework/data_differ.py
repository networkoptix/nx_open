#!/usr/bin/env python2

'''Specialized differ for data returned by mediaserver methods'''

# TODO: switch to 'yield' and 'yield from' when moved to python3

import argparse
from collections import namedtuple
from itertools import izip_longest
import logging
import json
from pathlib2 import Path
import sys

_logger = logging.getLogger(__name__)


def path2str(path):
    return '/'.join(map(str, path))


class Diff(object):

    def __init__(self, path, element_name, action, x=None, y=None, message=None):
        self.path = path
        self.element_name = element_name
        self.action = action
        self.x = x
        self.y = y
        self.message = message

    @property
    def _key(self):
        return (tuple(self.path), self.element_name, self.action, self.x, self.y)

    def __eq__(self, other):
        return self._key == other._key

    def __hash__(self):
        return hash(self._key)

    def __repr__(self):
        return '<Diff: {}>'.format(self)

    def __str__(self):
        return '/{}: {} {} (x={!r}, y={!r}): {}'.format(
               self.path_str,
               self.element_name or 'element',
               self.action,
               self.x,
               self.y,
               self.message or '',
               )

    @property
    def path_str(self):
        return path2str(self.path)


_KeyInfo = namedtuple('_KeyInfo', 'name key_elements name_elements')


class _PathPattern(object):

    def __init__(self, pattern_str=None):
        self._pattern = pattern_str.split('/') if pattern_str else []

    def match(self, path):
        if not self._pattern and not path:
            return True
        for pattern, id in izip_longest(self._pattern, path):
            if pattern != '*' and id != pattern:
                return False
        return True


class _DataDiffer(object):

    def __init__(self, name, key_map=None):
        self.name = name
        self._key_map = key_map or []  # (_PathPattern, _KeyInfo) list

    def __str__(self):
        return self.name

    def diff(self, x, y, path=None):
        return self._diff(path or [], x, y)

    def _diff(self, path, x, y):
        _logger.debug('Diff: checking /%s: x=%r y=%r', path2str(path), x, y)
        if type(x) != type(y):
            x_type_name = type(x).__name__
            y_type_name = type(y).__name__
            message = 'Different element types: x has %s, y has %s' % (x_type_name, y_type_name)
            return [Diff(path, None, 'changed', x, y, message)]
        if isinstance(x, (list, tuple)):
            return self._diff_seq(path, x, y)
        if isinstance(x, dict):
            return self._diff_dict(path, x, y)
        if isinstance(x, (int, long, str, unicode)):
            return self._diff_primitive(path, x, y)
        assert False, 'Unsupported element type: %s' % type(x)

    def _diff_primitive(self, path, x, y):
        if x != y:
            return [Diff(path, type(x).__name__, 'changed', x, y, 'x (%r) != y (%r)' % (x, y))]
        else:
            return []

    def _find_key_info(self, path):
        for pattern, key_info in self._key_map:
            if pattern.match(path):
                return key_info
        return None

    def _diff_seq(self, path, x, y):
        key_info = self._find_key_info(path)
        if key_info:
            return self._diff_keyed_seq(path, x, y, key_info)
        else:
            return self._diff_non_keyed_seq(path, x, y)

    def _diff_non_keyed_seq(self, path, x_seq, y_seq):
        result_diff_list = []
        for i, (x, y) in enumerate(zip(x_seq, y_seq)):
            diff_list = self._diff(path + [i], x, y)
            if diff_list:
                result_diff_list += diff_list
        if len(x_seq) != len(y_seq):
            result_diff_list += [
                Diff(path, 'sequence', 'changed', 
                     message='Sequence length does not match: x has %d elements, y has %d' % (len(x_seq), len(y_seq)))
                ]
        return result_diff_list

    def _diff_keyed_seq(self, path, x_seq, y_seq, key_info):

        def get_attr(element, path):
            value = element
            for attr in path.split('.'):
                value = value[attr]
            return value

        def element_key(element):
            key = [get_attr(element, attr) for attr in key_info.key_elements]
            if len(key_info.key_elements) > 1:
                return '|'.join(map(str, key))
            else:
                return key[0]

        def element_name(element):
            name = [get_attr(element, attr) for attr in key_info.name_elements]
            if len(name) > 1:
                return '|'.join(map(str, name))
            else:
                return name[0]

        diff_list = []
        x_dict = {element_key(element): element for element in x_seq}
        y_dict = {element_key(element): element for element in y_seq}
        for key in sorted(set(x_dict) - set(y_dict)):
            element = x_dict[key]
            diff_list.append(Diff(path, key_info.name, 'removed', element_name(element), None, element))
        for key in sorted(set(y_dict) - set(x_dict)):
            element = y_dict[key]
            diff_list.append(Diff(path, key_info.name, 'added', None, element_name(element), element))
        for key in sorted(set(x_dict) & set(y_dict)):
            name = element_name(x_dict[key])
            diff_list += self._diff(path + [name], x_dict[key], y_dict[key])
        return diff_list

    def _diff_dict(self, path, x_dict, y_dict):
        diff_list = []
        for key in sorted(set(x_dict) - set(y_dict)):
            diff_list.append(Diff(
                path, 'dict', 'removed', key, None, 'Element is removed: %s=%r' % (key, x_dict[key])))
        for key in sorted(set(y_dict) - set(x_dict)):
            diff_list.append(Diff(
                path, 'dict', 'added', None, key, 'Element is added: %s=%r' % (key, y_dict[key])))
        for key in sorted(set(x_dict) & set(y_dict)):
            diff_list += self._diff(path + [key], x_dict[key], y_dict[key])
        return diff_list


raw_differ = _DataDiffer('raw')

full_info_differ = _DataDiffer('full-info', [
    (_PathPattern('servers'), _KeyInfo('server', ['id'], ['name'])),
    (_PathPattern('users'), _KeyInfo('user', ['id'], ['name'])),
    (_PathPattern('storages'), _KeyInfo('storage', ['id'], ['name'])),
    (_PathPattern('cameras'), _KeyInfo('camera', ['id'], ['name'])),
    (_PathPattern('layouts'), _KeyInfo('layout', ['id'], ['name'])),
    (_PathPattern('layouts/*/items'), _KeyInfo('layout_item', ['id'], ['id'])),
    (_PathPattern('allProperties'), _KeyInfo('property', ['resourceId', 'name'], ['resourceId', 'name'])),
    ])

transaction_log_differ = _DataDiffer('transaction-log', [
    (_PathPattern(), _KeyInfo('transaction',
                    ['tran.peerID', 'tran.persistentInfo.dbID', 'tran.persistentInfo.sequence'],
                    ['tranGuid', 'tran.command'])),
    ])


def log_diff_list(log_fn, diff_list):
    for diff in diff_list:
        log_fn('> %s', diff)


# command-line interface for data differ:

_differ_map = {
    differ.name: differ for differ in [
        raw_differ,
        full_info_differ,
        transaction_log_differ,
    ]}


def _differ(value):
    differ = _differ_map.get(value)
    if not differ:
        raise argparse.ArgumentTypeError('%r is not an known differ. Known are: %r' % (value, _differ_map.keys()))
    return differ

def _dict_file(value):
    path = Path(value).expanduser()
    if not path.is_file():
        raise argparse.ArgumentTypeError('%s is not an existing file' % path)
    with path.open() as f:
         return json.load(f)


def _differ_util():
    parser = argparse.ArgumentParser()
    parser.add_argument('differ', type=_differ, help='Differ to use. One of %r' % _differ_map.keys())
    parser.add_argument('x', type=_dict_file, help='Path to first json file.')
    parser.add_argument('y', type=_dict_file, help='Path to second json file.')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose logging mode.')
    args = parser.parse_args()

    format = '%(asctime)-15s %(message)s'
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO, format=format)

    diff_list = args.differ.diff(args.x, args.y)
    _logger.info('Diff contains %d elements', len(diff_list))
    log_diff_list(_logger.info, diff_list)


if __name__ == '__main__':
    _differ_util()
