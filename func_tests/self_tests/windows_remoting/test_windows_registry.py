import pytest

from framework.os_access.windows_remoting.registry import Type, WindowsRegistry


@pytest.fixture()
def windows_registry(winrm):
    return WindowsRegistry(winrm)


def test_list_empty_key(windows_registry):  # type: (WindowsRegistry) -> None
    path = u'HKEY_LOCAL_MACHINE\\SOFTWARE'
    assert not windows_registry.key(path).list_values()


def test_list_key(windows_registry):  # type: (WindowsRegistry) -> None
    path = u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DateTime\\Servers'
    assert windows_registry.key(path).list_values()


def test_create_set_get_delete(windows_registry):  # type: (WindowsRegistry) -> None
    key = windows_registry.key(u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Network Optix Functional Tests')
    key.create()
    value = key.make_value(u'Dummy Value', Type.from_name(u'REG_SZ'))
    data = u'Dummy Data'
    value.set_data(data)
    assert value.get_data() == data
    key.delete()
