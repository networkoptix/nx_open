import logging
from pprint import pformat

_logger = logging.getLogger(__name__)


class Hive(object):
    @classmethod
    def possible_hives(cls):
        try:
            return cls._possible_hives
        except AttributeError:
            # Look for constants in documentation of methods of the StdRegProv class. E.g. of EnumValues method.
            # See: https://msdn.microsoft.com/en-us/library/aa390388(v=vs.85).aspx
            cls._possible_hives = [
                cls('HKEY_CLASSES_ROOT', 2147483648, 'HKCR'),
                cls('HKEY_CURRENT_USER', 2147483649, 'HKCU'),
                cls('HKEY_LOCAL_MACHINE', 2147483650, 'HKLM'),
                cls('HKEY_USERS', 2147483651, 'HKU'),
                cls('HKEY_CURRENT_CONFIG', 2147483653, 'HKCC'),
                ]
            return cls._possible_hives

    def __init__(self, name, int_value, abbr):
        self.int_value = int_value
        self.name = name
        self.abbr = abbr

    def __repr__(self):
        return '<{!s}>'.format(self.abbr)

    @classmethod
    def from_name(cls, name):
        for hive in cls.possible_hives():
            if name in (hive.name, hive.abbr):
                return hive
        raise ValueError("No hive with name {!r}".format(name))


class Type(object):
    """Normal class to allow autocomplete in PyCharm"""

    @classmethod
    def possible_types(cls):
        try:
            return cls._possible_types
        except AttributeError:
            cls._possible_types = [
                cls(1, 'REG_SZ', u'GetStringValue', u'SetStringValue', u'sValue'),
                cls(2, 'REG_EXPAND_SZ', u'GetExpandedStringValue', u'SetExpandedStringValue', u'sValue'),
                cls(3, 'REG_BINARY', u'GetBinaryValue', u'SetBinaryValue', u'uValue'),
                cls(4, 'REG_DWORD', u'GetDWORDValue', u'SetDWORDValue', u'uValue'),
                cls(7, 'REG_MULTI_SZ', u'GetMultiStringValue', u'SetMultiStringValue', u'sValue'),
                cls(11, 'REG_QWORD', u'GetQWORDValue', u'SetQWORDValue', u'uValue'),
                ]
            return cls._possible_types

    def __init__(self, int_value, name, getter_name, setter_name, parameter_name):
        self.int_value = int_value
        self.name = name
        self.getter_name = getter_name
        self.setter_name = setter_name
        self.parameter_name = parameter_name

    @classmethod
    def from_int(cls, int_value):
        for type in cls.possible_types():
            if int_value == type.int_value:
                return type
        raise ValueError("No type with int value {!r}".format(int_value))

    @classmethod
    def from_name(cls, name):
        for type in cls.possible_types():
            if name == type.name:
                return type
        raise ValueError("No type with name {!r}".format(name))

    @classmethod
    def guess(cls, name, data):
        """Guess type when value is new, use simple heuristic"""
        if isinstance(data, (bool, int)):
            if 'time' in name.lower():
                return cls.from_name('REG_QWORD')
            return cls.from_name('REG_DWORD')
        return cls.from_name('REG_SZ')


class _WindowsRegistryValue(object):
    def __init__(self, query, hive, key, name, type):
        self.query = query
        self.hive = hive  # type: Hive
        self.key = key  # type: str
        self.type = type  # type: Type
        self.name = name  # type: str

    def __repr__(self):
        return '<Value {}/{}/{} of type {}>'.format(self.hive, self.key, self.name, self.type.name)

    def get_data(self):
        response = self.query.invoke_method(self.type.getter_name, {
            u'hDefKey': self.hive.int_value, u'sSubKeyName': self.key, u'sValueName': self.name,
            })
        _logger.debug("%s response: %s", self.type.getter_name, pformat(response))
        value = response[self.type.parameter_name]
        _logger.debug('Value %s: %r', self.name, value)
        return value

    def set_data(self, new_data):
        response = self.query.invoke_method(self.type.setter_name, {
            u'hDefKey': self.hive.int_value, u'sSubKeyName': self.key, u'sValueName': self.name,
            self.type.parameter_name: new_data,
            })
        _logger.debug("%s response: %s", self.type.setter_name, pformat(response))


class _WindowsRegistryKey(object):
    def __init__(self, query, hive, key):
        self.query = query
        self.hive = hive
        self.key = key

    def __repr__(self):
        return '<Key {}\\{}>'.format(self.hive.name, self.key)

    def make_value(self, name, type):
        return _WindowsRegistryValue(self.query, self.hive, self.key, name, type)

    def list_values(self):
        response = self.query.invoke_method(u'EnumValues', {
            u'hDefKey': self.hive.int_value, u'sSubKeyName': self.key})
        values = [
            self.make_value(name, Type.from_int(int(type_str)))
            for type_str, name
            in zip(response.get(u'Types', []), response.get(u'sNames', []))]
        return values

    def create(self):
        self.query.invoke_method(u'CreateKey', {
            u'hDefKey': self.hive.int_value, u'sSubKeyName': self.key})

    def delete(self):
        self.query.invoke_method(u'DeleteKey', {
            u'hDefKey': self.hive.int_value, u'sSubKeyName': self.key})

    def copy_values_to(self, another_key):
        """Don't copy subkeys"""
        for value in self.list_values():
            new_value = another_key.make_value(value.name, value.type)
            new_value.set_data(value.get_data())

    def update_values(self, new_data_dict):
        """When updating values, there's need to know their types, that's why they're listed first"""
        new_data_dict = new_data_dict.copy()
        for value in self.list_values():
            try:
                new_data = new_data_dict.pop(value.name)
            except KeyError:
                _logger.debug("Left intact: %r", value)
                continue
            _logger.debug("Will be updated: %r <- %r", value, new_data)
            value.set_data(new_data)
        for name, new_data in new_data_dict.items():
            type_guess = Type.guess(name, new_data)
            value = self.make_value(name, type_guess)
            _logger.debug("Will be set: %r <- %r", value, new_data)
            value.set_data(new_data)


class WindowsRegistry(object):
    def __init__(self, winrm):
        self.query = winrm.wmi_query(u'StdRegProv', {})

    def key(self, path):
        hive_str, key = path.split('\\', 1)
        hive = Hive.from_name(hive_str)
        return _WindowsRegistryKey(self.query, hive, key)
