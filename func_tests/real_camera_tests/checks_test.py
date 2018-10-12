import pytest

from .checks import Failure, Halt, Success, expect_values


def _exception_failure(**kwargs):
    try:
        raise KeyError(7)
    except KeyError:
        failure = Failure(is_exception=True, **kwargs)
        failure.exception = [failure.exception[-1]]  # Remove stack.
        return failure


@pytest.mark.parametrize("result, report", [
    (Success(), {'status': 'success'}),
    (Failure(errors=['shit', 'happens']), {'status': 'failure', 'errors': ['shit', 'happens']}),
    (Failure(errors=['shit happens']), {'status': 'failure', 'errors': ['shit happens']}),
    (Failure(errors=['shit happens']), {'status': 'failure', 'errors': ['shit happens']}),
    (_exception_failure(), {'status': 'failure', 'exception': ['KeyError: 7']}),
    (_exception_failure(errors='shit'),
        {'status': 'failure', 'exception': ['KeyError: 7'], 'errors': ['shit']}),
    (Halt('waiting'), {'status': 'halt', 'message': 'waiting'}),
])
def test_results(result, report):
    assert repr(result.details) == repr(report)


@pytest.mark.parametrize("expected, actual", [
    ({'hello': 'world'}, {'hello': 'world', 'another': 'value'}),
    ({'top.mid.bottom': 777}, {'top': {'mid': {'bottom': 777}}}),
    ({'number1': [5, 10], 'number2': 6}, {'number1': 7, 'number2': 6}),
    ({'encoderIndex': 'primary', 'codec': 'MJPEG'}, {'encoderIndex': 0, 'codec': 8}),
    ({'id=b': {'v': 2}}, [{'id': 'a', 'v': 1}, {'id': 'b', 'v': 2}]),
    ({'path.id=a': {'v': 7}}, {'path': [{'id': 'a', 'v': 7}]}),
    ({'id=x.y': {'v': 7}}, [{'id': 'x.z', 'v': 5}, {'id': 'x.y', 'v': 7}]),
    ({'!id=x.y': {'v': 7}}, {'id=x.y': {'v': 7}}),
])
def test_expect_values_success(expected, actual):
    result = expect_values(expected, actual)
    assert isinstance(result, Success), result


@pytest.mark.parametrize("expected, actual, errors", [
    ({'hello': 'world'}, {'hello': 'worldX'},
        ["'camera.hello' is 'worldX', expected 'world'"]),
    ({'hello': 'world'}, {'helloX': 'worldX'},
        ["'camera.hello' is not found, expected 'world'"]),
    ({'top.mid.bottom': 777}, {'top': {'mid': {'bottom': 888}}},
        ["'camera.top.mid.bottom' is 888, expected 777"]),
    ({'top.mid.bottom': 777}, {'top': {'mid': 2}},
        ["'camera.top.mid' is 'int', expected {'bottom': 777}"]),
    ({'number1': [5, 10], 'number2': 6}, {'number1': 3, 'number2': 3},
        ["'camera.number1' is 3, expected range [5, 10]",
         "'camera.number2' is 3, expected 6"]),
    ({'encoderIndex': 'primary', 'codec': 'MJPEG'}, {'encoderIndex': 2, 'codec': 28},
        ["'camera.codec' is 'H264', expected 'MJPEG'",
         "'camera.encoderIndex' is 2, expected 'primary'"]),
    ({'id=b': {'v': 3}}, [{'id': 'a', 'v': 1}, {'id': 'b', 'v': 2}],
        ["'camera[id=b].v' is 2, expected 3"]),
    ({'id=b': {'v': 3}}, {'id': 'a', 'v': 1},
        ["'camera' is 'dict', expected a list"]),
    ({'path.id=a': {'v': 7}}, {'path': [{'id': 'a', 'v': 6}]},
        ["'camera.path[id=a].v' is 6, expected 7"]),
    ({'id=x.y': {'v': 7}}, [{'id': 'x.z', 'v': 5}, {'id': 'x.y', 'v': 6}],
        ["'camera[id=x.y].v' is 6, expected 7"]),
    ({'!id=x.y': {'v': 8}}, {'id=x.y': {'v': 7}},
        ["'camera.<id=x.y>.v' is 7, expected 8"]),
])
def test_expect_values_failure(expected, actual, errors):
    result = expect_values(expected, actual)
    assert isinstance(result, Failure), result
    assert set(errors) == set(result.errors)


@pytest.mark.parametrize("expected, actual", [
    ({'message': 'hello*'}, {'message': 'hello world'}),
    ({'message': 'hello*'}, {'message': 'hello life'}),
    ({'top.bottom': 'a?c'}, {'top': {'bottom': 'abc'}}),
    ({'top.bottom': 'a?c'}, {'top': {'bottom': 'adc'}}),
    ({'id=x*': {'v': 3}}, [{'id': 'x7', 'v': 3}, {'id': 'y8', 'v': 2}]),
    ({'id=x*': {'v': 3}}, [{'id': 'x7', 'v': 3}, {'id': 'x8', 'v': 2}]),
])
def test_expect_values_fnmatch_success(expected, actual):
    result = expect_values(expected, actual, 'data', syntax='*')
    assert isinstance(result, Success), result


@pytest.mark.parametrize("expected, actual, errors", [
    ({'message': 'hello*'}, {'message': 'hi world'},
        ["'data.message' is 'hi world', expected 'hello*'"]),
    ({'top.bottom': 'a?c'}, {'top': {'bottom': 'abbc'}},
        ["'data.top.bottom' is 'abbc', expected 'a?c'"]),
    ({'top.bottom': 'a?c'}, {'top': {'bottom': 'abcd'}},
        ["'data.top.bottom' is 'abcd', expected 'a?c'"]),
    ({'id=x*': {'v': 3}}, [{'id': 'x7', 'v': 1}, {'id': 'y8', 'v': 2}],
        ["'data[id=x*].v' is 1, expected 3"]),
    ({'id=x*': {'v': 3}}, [{'id': 'w7', 'v': 3}, {'id': 'y8', 'v': 2}],
        ["'data' does not have item with 'id=x*'"]),
])
def test_expect_values_fnmatch_failure(expected, actual, errors):
    result = expect_values(expected, actual, 'data', syntax='*')
    assert isinstance(result, Failure), result
    assert set(errors) == set(result.errors)
