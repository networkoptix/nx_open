"""
Switched logging.

Allows switching/substituting subsystem logger using context managers and decorators.
Usage example:
Module 'master':

_logger = logging.getLogger('master')
@with_logger(_logger, 'slave')
def do_stuff():
    do_other_stuff()


Module 'slave':

_logger = SwitchedLogger('slave')
def do_other_stuff():
    _logger.info('Do stuff')

Here 'Do stuff' will have logger 'master.slave'.
But when is not being called from master, it will have just 'slave' logger.
"""

import inspect
import logging
from functools import partial, wraps

_logger = logging.getLogger(__name__)


_switched_loggers = {}


# https://stackoverflow.com/questions/9213600/function-acting-as-both-decorator-and-context-manager-in-python
# contextlib.ContextDecorator doesn't support class wrapping, so we use our own implementation here.
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

    def __call__(self, func_or_class):
        if inspect.isclass(func_or_class):
            return self._wrap_class(func_or_class)
        else:
            return self._wrap_func(func_or_class)

    def _wrap_func(self, func):
        @wraps(func)
        def decorated(*args, **kw):
            with self:
                return func(*args, **kw)
        return decorated

    def _wrap_class(self, cls):
        for attr_name in dir(cls):
            attr = getattr(cls, attr_name)
            if not attr_name.startswith('__') and inspect.ismethod(attr):
                setattr(cls, attr_name, self._wrap_func(attr))
        return cls


class SwitchedLogger(object):

    def __init__(self, name, context_name=None):
        self.name = name
        self._context_name = context_name or name
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
            return self._overrides[-1].getChild(self._context_name)
        else:
            return self._default_logger

    def getChild(self, name):
        return self._get_current_logger().getChild(name)
