from collections import OrderedDict
from textwrap import dedent

import pytest

from framework.serialize import dump, load


@pytest.mark.parametrize(
    'data',
    [
        OrderedDict([('a', 1), ('b', 2), ('c', 3)]),
        OrderedDict([('a', 1), ('c', 3), ('b', 2)]),
        OrderedDict([('c', 3), ('a', 1), ('b', 2)]),
        OrderedDict([('c', 3), ('b', 2), ('a', 1)]),
        OrderedDict([('b', 2), ('c', 3), ('a', 1)]),
        OrderedDict([('b', 2), ('a', 1), ('c', 3)]),
        ],
    ids=[
        'forward',
        'disorder-1',
        'disorder-2',
        'reverse',
        'disorder-3',
        'disorder-4',
        ]
    )
def test_ordered_dict_ordering(data):
    assert load(dump(data)) == data


@pytest.mark.parametrize(
    'representation',
    [
        dedent("""
            a: 1
            b: 2
        """).strip(),
        dedent("""
            a: 1
            b:
              ba: 2
              bb: 3
        """).strip(),
        ],
    ids=[
        'flat-dict',
        'nested-dict',
        ]
    )
def test_dict_representation(representation):
    assert dump(load(representation)).rstrip() == representation
