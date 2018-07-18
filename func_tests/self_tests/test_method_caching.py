from framework.method_caching import cached_getter, cached_property


def test_cached_getter():
    calls = [0]

    class GetterOwner(object):
        @cached_getter
        def get_value(self):
            calls[0] += 1
            return object()

    instance = GetterOwner()
    _ = instance.get_value
    assert calls[0] == 0, "Getter must not be actually called when taken as variable."
    value = instance.get_value()
    same_value = instance.get_value()
    assert calls[0] == 1, "Getter must be called at least once."
    assert value is same_value, "Getter must return same object each time."


def test_cached_property():
    calls = [0]

    class PropertyOwner(object):
        @cached_property
        def value(self):
            calls[0] += 1
            return object()

    instance = PropertyOwner()
    value = instance.value
    same_value = instance.value
    assert calls[0] == 1, "Getter must be called at least once."
    assert value is same_value, "Getter must return same object each time."
