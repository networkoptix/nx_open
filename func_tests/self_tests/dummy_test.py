import pytest


@pytest.fixture()
def tt(request):
    yield
    request.node.post_check_errors.append('oi')


def test_failing(tt):
    assert 1
