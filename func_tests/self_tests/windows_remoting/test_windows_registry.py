import pytest

from framework.os_access.windows_remoting.registry import WindowsRegistry


@pytest.fixture(scope='session')
def windows_registry(winrm):
    return WindowsRegistry(winrm)


def test_list_empty_key(windows_registry):
    path = u'HKEY_LOCAL_MACHINE\\SOFTWARE'
    assert not windows_registry.key(path).list_values()


def test_list_key(windows_registry):
    path = u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DateTime\\Servers'
    assert windows_registry.key(path).list_values()
