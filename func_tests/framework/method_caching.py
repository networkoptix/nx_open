from functools import wraps


def cached_getter(unbound_method):
    """Return cached result on second call and on

    Use this decorator to preserve callable syntax
    if actual result is retrieved from network or disk
    or with heavy calculations.

    Wrapping decorators are not preserved after first call."""

    @wraps(unbound_method)
    def decorated(instance):
        result = unbound_method(instance)

        @wraps(unbound_method)
        def shortcut():
            return result

        instance.__dict__[unbound_method.__name__] = shortcut
        return result

    return decorated


# noinspection PyPep8Naming
class cached_property(object):
    """Property calculated once

    Use this decorator to make a property
    when it's desirable to return same object each same,
    e.g. when returning a type or mutable object.
    Don't use this decorator when getting involves working
    with network, disk or heavy calculations.

    Wrapping decorators are not preserved after first call."""

    def __init__(self, unbound_method):
        self.__doc__ = getattr(unbound_method, "__doc__")
        self._unbound_method = unbound_method

    def __get__(self, instance, owner):
        if instance is None:
            return self

        result = self._unbound_method(instance)
        instance.__dict__[self._unbound_method.__name__] = result
        return result
