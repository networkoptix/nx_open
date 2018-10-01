import pytest

from framework.ini_config import IniConfig, NameNotFound


def test_ini_config(os_access_temp_dir):
    path = os_access_temp_dir / 'dummy.ini'
    ini = IniConfig(path)
    ini.set('a', '1')
    assert ini.get('a') == '1'
    ini.reload()
    assert ini.get('a') == '1'
    ini = IniConfig(path)
    assert ini.get('a') == '1'
    ini.delete('a')
    pytest.raises(NameNotFound, ini.get, 'a')


def test_spaces(os_access_temp_dir):
    path = os_access_temp_dir / 'dummy_with_spaces.ini'
    path.write_text(u'  a  =  1  \n b = \nc=  2 3 4  ')
    ini = IniConfig(path)
    assert ini.get('a') == '1'
    assert ini.get('b') == ''
    assert ini.get('c') == '2 3 4'


def test_quotes(os_access_temp_dir):
    path = os_access_temp_dir / 'dummy_with_quotes.ini'
    path.write_text(u'  a  =  "  1  "  \nb=  "  "  ')
    ini = IniConfig(path)
    assert ini.get('a') == '  1  '
    assert ini.get('b') == '  '
