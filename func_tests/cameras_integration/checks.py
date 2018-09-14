import traceback
from abc import abstractmethod


class Result(object):
    @property
    @abstractmethod
    def report(self):
        pass


class Success(Result):
    @property
    def report(self):
        return dict(condition='success')


class Failure(Result):
    def __init__(self, errors=[], is_exception=False):
        assert errors or is_exception
        self.errors = errors if isinstance(errors, list) else [errors]
        self.exception = traceback.format_exc().strip() if is_exception else None

    def __repr__(self):
        return '<{} errors={}, exception={}>'.format(
            type(self).__name__, len(self.errors), bool(self.exception))

    @property
    def report(self):
        data = dict(
            errors=(self.errors[0] if len(self.errors) == 1 else self.errors),
            exception=self.exception.split('\n') if self.exception else self.exception)

        return dict(condition='failure', **{k: v for k, v in data.items() if v})


class Halt(Result):
    def __init__(self, message):  # types: (str) -> None
        self.message = message

    def __repr__(self):
        return '{}({!r})'.format(type(self).__name__, self.message)

    @property
    def report(self):
        return dict(condition='halt', **self.__dict__)


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


class Checker(object):
    def __init__(self):
        self._errors = []

    def add_error(self, error, *args):
        self._errors.append(str(error).format(*[repr(a) for a in args]))

    def result(self):  # type: () -> Result
        errors = self._errors
        self._errors = []
        return Failure(errors) if errors else Success()

    def expect_values(self, expected, actual, path='camera'):
        if isinstance(expected, dict):
            self.expect_dict(expected, actual, path)

        elif isinstance(expected, list):
            low, high = expected
            if not low <= actual <= high:
                self.add_error('{} is {}, expected to be in {}', path, actual, expected)

        elif expected != actual:
            self.add_error('{} is {}, expected {}', path, actual, expected)

        return not self._errors

    def expect_dict(self, expected, actual, path='camera'):
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
                    self.add_error('{} is {}, expected to be a list', path, actual_type)
                    continue

                item = self._search_item(*key.split('=', 1), items=actual)
                if item:
                    self.expect_values(expected_value, item, '{}[{}]'.format(path, key))
                else:
                    self.add_error('{} does not have item with {}', path, key)

            elif dot_position and (not equal_position or dot_position < equal_position):
                base_key, sub_key = key.split('.', 1)
                self.expect_values({base_key: {sub_key: expected_value}}, actual, path)

            else:
                if not isinstance(actual, dict):
                    self.add_error('{} is {}, expected to be a dict', path, actual_type)
                else:
                    self.expect_values(expected_value, self._get_key_value(key, actual), full_path)

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
    def _search_item(cls, key, value, items):
        for item in items:
            if str(cls._get_key_value(key, item)) == value:
                return item
