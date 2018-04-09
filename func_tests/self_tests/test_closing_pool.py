from collections import namedtuple
from contextlib import contextmanager

import pytest

from framework.pool import ClosingPool


@pytest.fixture()
def resources_state():
    return {}


class _ResourceFactory(object):
    _Resource = namedtuple('Resource', ['key', 'second_arg'])

    def __init__(self, _resources_state):
        self._resources_state = _resources_state

    @contextmanager
    def allocated_resource(self, key, second_arg):
        assert key not in self._resources_state
        self._resources_state[key] = 'allocated'
        try:
            yield self._Resource(key=key, second_arg=second_arg)
        finally:
            assert self._resources_state[key] == 'allocated'
            self._resources_state[key] = 'disposed'


@pytest.fixture()
def closing_pool(resources_state):
    special_second_args = {'key 1': 'second arg for key 1', 'key 2': 'second arg for key 2'}
    resource_factory = _ResourceFactory(resources_state)
    return ClosingPool(resource_factory.allocated_resource, special_second_args, 'default second arg')


def test_closing_pool_yield_value(closing_pool):
    with closing_pool as closing_pool_yielded_value:
        assert closing_pool is closing_pool_yielded_value


@pytest.fixture()
def entered_closing_pool(closing_pool):
    with closing_pool:
        yield closing_pool


def test_closing_pool_allocation(entered_closing_pool):
    allocated_resource_1 = entered_closing_pool.get('key 1')
    assert allocated_resource_1.key == 'key 1'
    assert allocated_resource_1.second_arg == 'second arg for key 1'


def test_closing_pool_multiple_allocation(closing_pool, resources_state):
    with closing_pool:
        closing_pool.get('key 1')
        assert resources_state['key 1'] == 'allocated'
        assert 'key 2' not in resources_state
        closing_pool.get('key 2')
        assert resources_state['key 1'] == 'allocated'
        assert resources_state['key 2'] == 'allocated'
    assert resources_state['key 1'] == 'disposed'
    assert resources_state['key 2'] == 'disposed'
