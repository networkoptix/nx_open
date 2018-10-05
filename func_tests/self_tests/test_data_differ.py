import logging
import json

import pytest
from pathlib2 import Path

from framework.data_differ import (
    Diff,
    _PathPattern,
    raw_differ,
    full_info_differ,
    transaction_log_differ,
    log_diff_list,
    )

_logger = logging.getLogger(__name__)


@pytest.mark.parametrize(('pattern', 'path', 'outcome'), [
    ('', [], True),
    ('abc', ['abc'], True),
    ('*', ['abc'], True),
    ('abc/def/xyz', ['abc', 'def', 'xyz'], True),
    ('abc/*/xyz', ['abc', 'def', 'xyz'], True),
    ('*/def/xyz', ['abc', 'def', 'xyz'], True),
    ('', ['abc'], False),
    ('abc', [], False),
    ('abc', ['abX'], False),
    ('*', ['abc', 'def'], False),
    ('abc/def/xyz', ['abc', 'deX', 'xyz'], False),
    ('abc/*/xyz', ['abX', 'def', 'xyz'], False),
    ('*/def/xyz', ['abc', 'def', 'xyX'], False),
    ])
def test_path_pattern(pattern, path, outcome):
    assert _PathPattern(pattern).match(path) == outcome


def test_diff_simple_list():
    x = [1, 2, 3, 4, 5]
    y = [1, 2, 10]
    diff_list = raw_differ.diff(x, y)
    log_diff_list(_logger.info, diff_list)
    assert diff_list == [
        Diff([2], 'int', 'changed', 3, 10),
        Diff([], 'sequence', 'changed'),
        ]


def test_diff_simple_dict():
    x = dict(key_1=1, key_2=2, key_3=3, key_4=4)
    y = dict(key_1=11, key_2=2, key_3=3, key_5=5)
    diff_list = raw_differ.diff(x, y)
    log_diff_list(_logger.info, diff_list)
    assert diff_list == [
        Diff([], 'dict', 'removed', x='key_4'),
        Diff([], 'dict', 'added', y='key_5'),
        Diff(['key_1'], 'int', 'changed', 1, 11),
        ]


def test_nested_element():
    x = dict(key_1=dict(key_2=dict(key_3='abc')))
    y = dict(key_1=dict(key_2=dict(key_3='def')))
    diff_list = raw_differ.diff(x, y)
    log_diff_list(_logger.info, diff_list)
    assert diff_list == [
        Diff(['key_1', 'key_2', 'key_3'], 'str', 'changed', 'abc', 'def'),
        ]


my_path = Path(__file__)
files_dir = my_path.with_name(my_path.stem + '_files')

def text_line_to_diff(line):
    path_str, element_name, action, x, y = line.split()
    path = filter(None, path_str.strip('/').split('/'))
    return Diff(path, element_name, action, json.loads(x), json.loads(y))

def load_full_info_base_name_list():
    for differ in [full_info_differ, transaction_log_differ]:
        for diff_path in files_dir.glob('{}-*-diff.txt'.format(differ.name)):
            base_name = diff_path.stem.rsplit('-', 1)[0]
            file_stem = base_name[len(differ.name) + 1:]
            yield (differ, file_stem)


@pytest.mark.parametrize('differ, file_stem', load_full_info_base_name_list(), ids='{0}'.format)
def test_differ(differ, file_stem):
    base_name = differ.name + '-' + file_stem
    x = json.loads(files_dir.joinpath(base_name + '-x').with_suffix('.json').read_text())
    y = json.loads(files_dir.joinpath(base_name + '-y').with_suffix('.json').read_text())
    diff_list_text = files_dir.joinpath(base_name + '-diff').with_suffix('.txt').read_text()
    expected_diff_list = map(text_line_to_diff, diff_list_text.rstrip().splitlines())
    diff_list = differ.diff(x, y)
    log_diff_list(_logger.info, diff_list)
    assert set(diff_list) == set(expected_diff_list)
