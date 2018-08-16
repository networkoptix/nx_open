import pytest

from framework.ini_config import IniConfig, NameNotFound


def test_ini_config(os_access):
    path = os_access.Path('/tmp/func_tests_dummy.ini')
    ini = IniConfig(path)
    ini.set('a', '1')
    assert ini.get('a') == '1'
    ini.reload()
    assert ini.get('a') == '1'
    ini = IniConfig(path)
    assert ini.get('a') == '1'
    ini.delete('a')
    pytest.raises(NameNotFound, ini.get, 'a')
