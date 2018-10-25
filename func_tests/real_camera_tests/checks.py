import traceback
from fnmatch import fnmatch

from typing import Optional


class Result(object):
    @property
    def status(self):
        return type(self).__name__.lower()

    @property
    def details(self):
        return dict(status=self.status, **self._field_values())

    def __repr__(self):
        return '{}({})'.format(
            type(self).__name__,
            ', '.join(['{}={!r}'.format(k, v) for k, v in self._field_values().items()]))

    def _field_values(self):
        return {k: v for k, v in self.__dict__.items() if v}


class Success(Result):
    pass


class Failure(Result):
    def __init__(self, errors=None, exception=None):
            # types: (Optional[list], Optional[list]) -> None
        """At least 1 arg should be filled.
        """
        self.errors = errors if isinstance(errors, list) else ([errors] if errors else [])
        self.exception = exception or []
        if not self.errors and not self.exception:
            raise ValueError('No error description is provided')

    def with_more_errors(self, *errors):
        return Failure(self.errors + list(errors), self.exception)

    @staticmethod
    def from_current_exception():
        return Failure(exception=traceback.format_exc().strip().splitlines())


class Halt(Result):
    def __init__(self, message):  # types: (str) -> None
        self.message = message


def expect_values(expected, actual, *args, **kwargs):
    checker = Checker()
    checker.expect_values(expected, actual, *args, **kwargs)
    return checker.result()


def retry_expect_values(expected_result, call, exceptions=None, *args, **kwargs):
    while True:
        try:
            actual_result = call()

        except exceptions as error:
            yield Failure(str(error))

        else:
            if not expected_result:
                return

            result = expect_values(expected_result, actual_result, *args, **kwargs)
            if isinstance(result, Success):
                return

            yield result


def _compare(expected, actual, syntax=None):
    if not syntax:
        return expected == actual

    if syntax in ('*', '?', 'fn'):
        return fnmatch(str(actual), str(expected))

    raise KeyError('Unsupported syntax: {}'.format(syntax))


class Checker(object):
    def __init__(self):
        self._errors = []

    def add_error(self, error, *args):
        self._errors.append(str(error).format(*[repr(a) for a in args]))

    def result(self):  # type: () -> Result
        errors = self._errors
        self._errors = []
        return Failure(errors) if errors else Success()

    def expect_values(self, expected, actual, path='camera', syntax=None):
        if actual is None and expected is not None:
            self.add_error('{} is not found, expected {}', path, expected)

        elif isinstance(expected, dict):
            self.expect_dict(expected, actual, path, syntax)

        elif isinstance(expected, list):
            low, high = expected
            if not low <= actual <= high:
                self.add_error('{} is {}, expected range {}', path, actual, expected)

        elif not _compare(expected, actual, syntax):
            self.add_error('{} is {}, expected {}', path, actual, expected)

        return not self._errors

    def expect_dict(self, expected, actual, path='camera', syntax=None):
        actual_type = type(actual).__name__
        for key, expected_value in expected.items():
            if key.startswith('!'):
                key = key[1:]
                equal_position, dot_position = 0, 0
                full_path = '{}.<{}>'.format(path, key)
            else:
                equal_position = key.find('=') + 1
                dot_position = key.find('.') + 1
                full_path = '{}.{}'.format(path, key)

            if equal_position and (not dot_position or equal_position < dot_position):
                if not isinstance(actual, list):
                    self.add_error('{} is {}, expected a list', path, actual_type)
                    continue

                item = self._search_item(*key.split('=', 1), items=actual, syntax=syntax)
                if item:
                    self.expect_values(expected_value, item, '{}[{}]'.format(path, key), syntax)
                else:
                    self.add_error('{} does not have item with {}', path, key)

            elif dot_position and (not equal_position or dot_position < equal_position):
                base_key, sub_key = key.split('.', 1)
                self.expect_values({base_key: {sub_key: expected_value}}, actual, path, syntax)

            else:
                if not isinstance(actual, dict):
                    self.add_error('{} is {}, expected {}', path, actual_type, expected)
                else:
                    self.expect_values(
                        expected_value, self._get_key_value(key, actual), full_path, syntax)

    # These are values that may be different between VMS version, so we normalize them.
    _KEY_VALUE_FIXES = {
        'encoderIndex': {0: 'primary', 1: 'secondary'},
        'codec': {8: 'MJPEG', 28: 'H264', 'HEVC': 'H265', 'acc': 'ACC'},
    }

    @classmethod
    def _get_key_value(cls, key, values):
        value = values.get(key)
        try:
            return cls._KEY_VALUE_FIXES[key][value]
        except (KeyError, TypeError):
            return value

    @classmethod
    def _search_item(cls, key, value, items, syntax=None):
        for item in items:
            if _compare(value, str(cls._get_key_value(key, item)), syntax):
                return item
