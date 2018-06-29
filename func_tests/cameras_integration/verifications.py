import traceback
import logging

from framework.camera import Camera

STAGES = []

_logger = logging.getLogger(__name__)


class Result(object):
    def __init__(self, errors=[], is_exception=False):
        self.errors = errors
        self.exception = traceback.format_exc().strip() if is_exception else None

    def __nonzero__(self):
        return not self.errors and not self.exception

    def __repr__(self):
        return repr(self.to_dict())

    def to_dict(self):
        d = dict(status=('ok' if self else 'error'))
        if self.errors:
            d['errors'] = self.errors
        if self.exception:
            d['exception'] = self.exception.split('\n')
        return d


def _register_stage(action, is_essential=False, retry_count=0, retry_delay_s=10):
    def stage(server, camera_id, **kwargs):
        verifier = Verifier(server, camera_id)
        try:
            action(verifier, **kwargs)
        except:
            return Result(is_exception=True, errors=verifier.errors)
        if verifier.errors:
            return Result(errors=verifier.errors)
        return Result()

    stage.name = action.__name__
    stage.is_essential = is_essential
    stage.retry_count = retry_count
    stage.retry_delay_s = retry_delay_s
    STAGES.append(stage)
    return stage


def _stage(**kwargs):
    return lambda c: _register_stage(c, **kwargs)


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
                item = self._search_item(*key.split('='), items=actual)
                if item:
                    self.expect_values(expected_value, item, '{}[{}]'.format(path, key))
                else:
                    self.errors.append('{} does not have item with {}'.format(path, key))
            else:
                value_path = '{}.{}'.format(path, key)
                if key in actual:
                    self.expect_values(expected_value, actual[key], value_path)
                else:
                    self.errors.append('{} does not exit'.format(value_path))

    @staticmethod
    def _search_item(key, value, items):
        for item in items:
            if str(item.get(key)) == value:
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
    if status == 'Unauthorized':
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
