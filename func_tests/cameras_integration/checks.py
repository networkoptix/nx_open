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
        return dict(status='success')


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

        return dict(status='failure', **{k: v for k, v in data.items() if v})


class Halt(Result):
    def __init__(self, message):  # types: (str) -> None
        self.message = message

    def __repr__(self):
        return '{}({})'.format(type(self).__name__, repr(message))

    @property
    def report(self):
        return dict(status='halt', **self.__dict__)


def expect_values(expected, actual):
    checker = Checker()
    checker.expect_values(expected, actual)
    return checker.result()


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
            if '.' in key:
                base_key, sub_key = key.split('.', 1)
                self.expect_values({sub_key: expected_value}, actual.get(base_key),
                                   '{}.{}'.format(path, base_key))
            elif '=' in key:
                if not isinstance(actual, list):
                    self.add_error('{} is {}, expected to be a list', path, actual_type)
                    continue

                item = self._search_item(*key.split('=', 1), items=actual)
                if item:
                    self.expect_values(expected_value, item, '{}[{}]'.format(path, key))
                else:
                    self.add_error('{} does not have item with {}', path, key)
            else:
                full_path = '{}.{}'.format(path, key)
                if not isinstance(actual, dict):
                    self.add_error('{} is {}, expected to be a dict', full_path, actual_type)
                else:
                    self.expect_values(expected_value, actual.get(key), full_path)

    # These are values that may be different between VMS version, so we normalize them.
    _KEY_VALUE_FIXES = {
        'encoderIndex': {0: 'primary', 1: 'secondary'},
    }

    @classmethod
    def _fix_key_value(cls, key, value):
        return cls._KEY_VALUE_FIXES.get(key, {}).get(value, value)

    @classmethod
    def _search_item(cls, key, value, items):
        for item in items:
            if str(cls._fix_key_value(key, item.get(key))) == value:
                return item
