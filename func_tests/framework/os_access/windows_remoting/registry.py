import logging
from collections import namedtuple
from pprint import pformat

from framework.os_access.windows_remoting import CIMQuery

_logger = logging.getLogger(__name__)

_keys = {
    # Look for constants in documentation of methods of the StdRegProv class. E.g. of EnumValues method.
    # See: https://msdn.microsoft.com/en-us/library/aa390388(v=vs.85).aspx
    u'HKEY_CLASSES_ROOT': 2147483648,
    u'HKEY_CURRENT_USER': 2147483649,
    u'HKEY_LOCAL_MACHINE': 2147483650,
    u'HKEY_USERS': 2147483651,
    u'HKEY_CURRENT_CONFIG': 2147483653,
    }

Type = namedtuple('Type', ['value', 'define', 'getter', 'setter', 'parameter'])

string_type = Type(u'1', 'REG_SZ', u'GetStringValue', u'SetStringValue', u'sValue')
expanded_string_type = Type(u'2', 'REG_EXPAND_SZ', u'GetExpandedStringValue', u'SetExpandedStringValue', u'sValue')
binary_type = Type(u'3', 'REG_BINARY', u'GetBinaryValue', u'SetBinaryValue', u'uValue')
dword_type = Type(u'4', 'REG_DWORD', u'GetDWORDValue', u'SetDWORDValue', u'uValue')
multi_string_type = Type(u'7', 'REG_MULTI_SZ', u'GetMultiStringValue', u'SetMultiStringValue', u'sValue')
qword_type = Type(u'11', 'REG_QWORD', u'GetQWORDValue', u'SetQWORDValue', u'uValue')

_types = {
    u'1': string_type,
    u'2': expanded_string_type,
    u'3': binary_type,
    u'4': dword_type,
    u'7': multi_string_type,
    u'11': qword_type,
    }


class Name(object):
    def __init__(self, key, name, type):
        self.key = key  # type: Key
        self.query = key.query  # type: CIMQuery
        self.type = type  # type: Type
        self.name = name  # type: str

    def __repr__(self):
        return '<Name {} of type {}>'.format(self.name, self.type.define)

    def value(self):
        response = self.query.invoke_method(self.type.getter, {
            u'hDefKey': self.key.key_int,
            u'sSubKeyName': self.key.sub_key,
            u'sValueName': self.name,
            })
        _logger.debug("%s response: %s", self.type.getter, pformat(response))
        value = response[self.type.parameter]
        _logger.debug('Value %s: %r', self.name, value)
        return value

    def copy(self, another_key):
        value = self.value()
        response = self.query.invoke_method(self.type.setter, {
            u'hDefKey': another_key.key_int,
            u'sSubKeyName': another_key.sub_key,
            u'sValueName': self.name,
            self.type.parameter: value,
            })
        _logger.debug("%s response: %s", self.type.setter, pformat(response))


class Key(object):
    def __init__(self, query, path):
        self.query = query
        self.key, self.sub_key = path.split('\\', 1)
        self.key_int = _keys[self.key]

    def list_values(self):
        response = self.query.invoke_method(u'EnumValues', {
            u'hDefKey': self.key_int,
            u'sSubKeyName': self.sub_key})
        _logger.debug("EnumValues response: %s", pformat(response))
        names = [
            Name(self, value_name, _types[value_type])
            for value_type, value_name
            in zip(response.get(u'Types', []), response.get(u'sNames', []))]
        return names

    def create(self):
        response = self.query.invoke_method(u'CreateKey', {
            u'hDefKey': self.key_int,
            u'sSubKeyName': self.sub_key})
        _logger.debug("CreateKey response: %s", pformat(response))


class WindowsRegistry(object):
    def __init__(self, winrm):
        self.query = winrm.wmi_query(u'StdRegProv', {})

    def key(self, path):
        return Key(self.query, path)
