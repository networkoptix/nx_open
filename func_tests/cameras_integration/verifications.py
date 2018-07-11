import traceback
import logging

from framework.camera import Camera
from framework.rest_api import HttpError

STAGES = []  # Filled by _stage decorator.

_logger = logging.getLogger(__name__)


def _stage(is_essential=False, retry_count=0, retry_delay_s=10):
    def decorator(action):
        # TODO: Better make it a helper class.
        def stage(server, camera_id, **kwargs):
            verifier = Verifier(server, camera_id)
            try:
                action(verifier, **kwargs)
            except HttpError as error:
                verifier.errors.append(str(error))
            except Exception:
                return Result.on_exception(errors=verifier.errors)
            if verifier.errors:
                return Result(errors=verifier.errors)
            return Result()

        stage.name = action.__name__
        stage.is_essential = is_essential
        stage.retry_count = retry_count
        stage.retry_delay_s = retry_delay_s
        STAGES.append(stage)
        return stage

    return decorator


_KEY_VALUE_FIXES = {
    'encoderIndex': {0: 'primary', 1: 'secondary'},
}


def _fix_key_value(key, value):
    return _KEY_VALUE_FIXES.get(key, {}).get(value, value)


class Result(object):
    def __init__(self, errors=[], exception_traceback=[]):
        self.errors = errors
        self.exception_traceback = exception_traceback

    def __nonzero__(self):
        return not self.errors and not self.exception_traceback

    def __repr__(self):
        if self:
            return '{}()'.format(type(self).__name__)
        return '{}(errors={}, exception_traceback={})'.format(
            type(self).__name__, repr(self.errors), repr(self.exception_traceback))

    def to_dict(self):
        d = dict(status=('ok' if self else 'error'))
        if self.errors:
            d['errors'] = self.errors
        if self.exception_traceback:
            d['exception'] = self.exception_traceback
        return d

    @classmethod
    def on_exception(cls, *args, **kwargs):
        trace = traceback.format_exc().strip().split('\n')
        return cls(exception_traceback=trace, *args, **kwargs)


class Verifier(object):
    def __init__(self, server, id):
        self.server = server
        self.data = self.server.get_camera(id)
        self.camera = Camera(None, None, self.data['name'], self.data['mac'], id)
        self.errors = []

    def expect_values(self, expected, actual, path='camera'):
        if isinstance(expected, dict):
            self.expect_dict(expected, actual, path)
        elif isinstance(expected, list):
            min, max = expected
            if actual < min:
                self.errors.append('{} is {}, expected >= {}'.format(path, repr(actual), repr(min)))
            elif actual > max:
                self.errors.append('{} is {}, expected <= {}'.format(path, repr(actual), repr(max)))
        elif expected != actual:
            self.errors.append('{} is {}, expected {}'.format(path, repr(actual), repr(expected)))

    def expect_dict(self, expected, actual, path='camera'):
        for key, expected_value in expected.iteritems():
            if '=' in key:
                if not isinstance(actual, list):
                    self.errors.append('{}.{} is {}, expected to be a list'.format(
                        path, key, type(actual).__name__))
                    continue

                item = self._search_item(*key.split('=', 1), items=actual)
                if item:
                    self.expect_values(expected_value, item, '{}[{}]'.format(path, key))
                    continue

                self.errors.append('{} does not have item with {}'.format(path, key))

            else:
                if not isinstance(actual, dict):
                    self.errors.append('{}.{} is {}, expected to be a dict'.format(
                        path, key, type(actual).__name__))
                    continue

                self.expect_values(expected_value, actual.get(key), '{}.{}'.format(path, key))

    @staticmethod
    def _search_item(key, value, items):
        for item in items:
            if str(_fix_key_value(key, item.get(key))) == value:
                return item


@_stage(is_essential=True)
def discovery(verifier, **kwargs):
    if 'mac' not in kwargs:
        kwargs['mac'] = kwargs['physicalId']
    if 'name' not in kwargs:
        kwargs['name'] = kwargs['model']
    verifier.expect_values(kwargs, verifier.data)


@_stage(is_essential=True, retry_count=10)
def authorization(verifier, login=None, password=None):
    status = verifier.data['status']
    if status == 'Online':
        return

    verifier.errors.append('Unexpected status: ' + status)
    if login and status == 'Unauthorized':
        verifier.server.set_camera_credentials(verifier.data['id'], login, password)


@_stage(retry_count=10)
def recording(verifier, **options):
    status = verifier.data['status']
    if status != 'Recording':
        verifier.errors.append('Unexpected status: ' + status)
        return verifier.server.start_recording_camera(verifier.camera, options=options)

    if not verifier.server.get_recorded_time_periods(verifier.camera):
        verifier.errors.append('No data is recorded')


@_stage(retry_count=5)
def attributes(verifier, **kwargs):
    verifier.expect_values(kwargs, verifier.data)
