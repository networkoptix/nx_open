import traceback
import logging

STAGES = []

_logger = logging.getLogger(__name__)


class Result(object):
    def __init__(self, errors=[], is_exception=False):
        self.errors = errors
        self.exception = traceback.format_exc() if is_exception else None

    def __nonzero__(self):
        return not self.errors and not self.exception

    def __repr__(self):
        return repr(self.to_dict())

    def to_dict(self):
        d = dict(status=('ok' if self else 'error'))
        if self.errors:
            d['errors'] = self.errors
        if self.exception:
            d['exception'] = self.exception.splitlines()
        return d


def _register_stage(action, is_essential=False, retry_count=0, retry_delay_s=None):
    assert bool(retry_count) == bool(retry_delay_s)

    def stage(server, camera_id, **kwargs):
        verifier = Verifier(server, camera_id)
        try:
            action(verifier, **kwargs)
        except (TypeError):
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
        self.errors = []

    def expect_value(self, name, *expected_values):
        actual_value = self.data.get(name)
        if actual_value not in expected_values:
            self.errors.append('Value of {} is {} not in {}'.format(
                repr(name), repr(actual_value), repr(expected_values)))


@_stage(is_essential=True)
def discovery(verifier, physicalId, mac='', vendor='', vendors=[], model='', models=[]):
    verifier.expect_value('physicalId', physicalId)
    verifier.expect_value('mac', mac or physicalId)
    verifier.expect_value('vendor', *(vendors or [vendor]))
    verifier.expect_value('model', *(models or [model]))


@_stage(is_essential=True, retry_count=10, retry_delay_s=10)
def authorization(verifier, login=None, password=None):
    status = verifier.data['status']
    if status == 'Online':
        return

    verifier.errors.append('Unexpected status: ' + status)
    if status == 'Unauthorized':
        verifier.server.set_camera_credentials(verifier.data['id'], login, password)


@_stage()
def attributes(verifier, **kwargs):
    for key, value in kwargs.iteritems():
        verifier.expect_value(key, value)







