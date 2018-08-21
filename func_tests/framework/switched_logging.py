"""
Switched logging.

Allows switching subsystem logger using context managers and decorators.
"""


import logging
from functools import partial, wraps

_logger = logging.getLogger(__name__)


_switched_loggers = {}


# https://stackoverflow.com/questions/9213600/function-acting-as-both-decorator-and-context-manager-in-python
# TODO: may be use @contextlib.contextmanager after move to python3
class with_logger(object):

    def __init__(self, parent_logger, child_logger_name):
        self._parent_logger = parent_logger
        self._child_logger_name = child_logger_name

    def __enter__(self):
        try:
            overrides = _switched_loggers[self._child_logger_name]
        except KeyError:
            assert False, 'Unknown child logger name: %r' % self._child_logger_name
        _logger.debug('Add parent log %r for %r', self._parent_logger.name, self._child_logger_name)
        overrides.append(self._parent_logger)

    def __exit__(self, *args, **kwds):
        _logger.debug('Remove parent log %r for %r', self._parent_logger.name, self._child_logger_name)
        overrides = _switched_loggers[self._child_logger_name]
        overrides.pop()

    def __call__(self, func):
        @wraps(func)
        def decorated(*args, **kw):
            with self:
                return func(*args, **kw)
        return decorated


class SwitchedLogger(object):

    def __init__(self, name):
        self._name = name
        self._default_logger = logging.getLogger(name)
        for level in [
                logging.DEBUG,
                logging.INFO,
                logging.WARNING,
                logging.ERROR,
                logging.CRITICAL,
                ]:
            level_name = logging.getLevelName(level)
            setattr(self, level_name.lower(), partial(self.log, level))
        self._overrides = []
        _switched_loggers[name] = self._overrides

    def log(self, level, msg, *args, **kw):
        self._get_current_logger().log(level, msg, *args, **kw)

    def exception(self, msg, *args, **kw):
        self._get_current_logger().exception(msg, *args, **kw)

    def _get_current_logger(self):
        if self._overrides:
            return self._overrides[-1].getChild(self._name)
        else:
            return self._default_logger

    def getChild(self, name):
        return self._get_current_logger().getChild(name)
