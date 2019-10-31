from typing import Optional, List, Dict

from vms_benchmark import exceptions


class NoConfigFile(exceptions.VmsBenchmarkError):
    pass


class InvalidConfigFileContent(exceptions.VmsBenchmarkError):
    pass


class ConfigOptionNotFound(exceptions.VmsBenchmarkError):
    pass


class InvalidConfigOption(exceptions.VmsBenchmarkError):
    pass


class InvalidConfigDefinition(Exception):
    pass


class ConfigParser:
    @staticmethod
    def _validate_option_definition(name: str, option_definition: dict, filename: str):
        if 'type' not in option_definition:
            raise InvalidConfigDefinition(
                f'Missing "type" key in option {name!r} definition for {filename!r}.')
        type_ = option_definition['type']

        if type_ not in ['str', 'int', 'bool', 'float', 'intList']:
            raise InvalidConfigDefinition(
                f'Invalid type {type!r} in option {name!r} definition for {filename!r}.')

        if type_ not in ['int', 'intList', 'float'] and 'range' in option_definition:
            raise InvalidConfigDefinition(
                f'Range specified for {type!r} option {name!r} definition for {filename!r}.')

    @staticmethod
    def _process_env_vars(string):
        import re
        from os import environ
        return re.sub(r"\$([a-zA-Z_][a-zA-Z_]+)",
            lambda match: environ.get(match.group(1), ''), string)

    @staticmethod
    def _load_file(filename, is_file_optional) -> Optional[dict]:
        try:
            file = open(filename)
        except FileNotFoundError:
            if is_file_optional:
                return None
            raise NoConfigFile(f"Config file {filename!r} not found.")

        options = {}
        line_number = 0
        for line in file.readlines():
            line_number += 1
            comment_start_pos = line.find('#')
            if comment_start_pos != -1:
                line = line[:comment_start_pos]
            line = line.strip()
            if not line.strip() or (
                    line.startswith('[') and line.endswith(']')):  # Ignore .ini sections.
                continue
            equals_pos = line.find('=')
            if equals_pos == -1:
                raise InvalidConfigFileContent(
                    f'Missing "=" in {filename!r}, line {line_number}.')
            name = line[:equals_pos].strip()
            if not name:
                raise InvalidConfigFileContent(
                    f'Missing option name in {filename!r}, line {line_number}.')
            if name in options:
                raise InvalidConfigFileContent(
                    f'Duplicate option {name!r} in {filename!r}, line {line_number}.')
            options[name] = ConfigParser._process_env_vars(line[equals_pos + 1:].strip())

        return options

    def __init__(self, filename, definition=None, is_file_optional=False):
        assert definition or not is_file_optional

        self.OPTIONS_FROM_FILE = ConfigParser._load_file(filename, is_file_optional)
        self.options = self.OPTIONS_FROM_FILE.copy() if self.OPTIONS_FROM_FILE else {}

        if definition:
            ConfigParser._set_option_values_using_definition(self.options, filename, definition)

    @staticmethod
    def _set_option_values_using_definition(options: dict, filename, definition: dict) -> None:
        for name in options:
            if name not in definition:
                raise InvalidConfigOption(f"Unexpected option {name!r} in {filename!r}.")

        for name, option_definition in definition.items():
            ConfigParser._validate_option_definition(name, option_definition, filename)
            value = options.get(name, None)
            if value:
                range_ = option_definition.get('range', None)
                options[name] = ConfigParser._option_value(
                    value, filename, name, option_definition['type'], range_)
            else:
                if 'default' not in option_definition:
                    raise ConfigOptionNotFound(
                        f"Mandatory option {name!r} is not defined in {filename!r}.")
                options[name] = option_definition['default']

    @staticmethod
    def _option_value(value, filename, name: str, type_: str, range_: Optional[list]):
        try:
            if type_ == "int":
                return ConfigParser._check_range(int(value), range_)
            if type_ == "float":
                return ConfigParser._check_range(float(value), range_)
            if type_ == "bool":
                return ConfigParser._bool_value(value)
            if type_ == 'intList':
                return ConfigParser._int_list_value(value, range_)
            if type_ == "str":
                return ConfigParser._str_value(value)
        except ValueError as e:
            raise InvalidConfigOption(
                f"Invalid option {name!r} value {value!r} in {filename!r}: {e}")
        raise InvalidConfigDefinition(
            f"Unexpected type {type_!r} in option {name!r} in the definition for {filename!r}.")

    @staticmethod
    def _check_range(value, range_: Optional[list]):
        if range_ is not None:
            if range_[0] is not None and value < range_[0]:
                raise ValueError(f"Must not be less than {range_[0]}.")
            elif range_[1] and value > range_[1]:
                raise ValueError(f"Must not be greater than {range_[1]}.")
        return value

    @staticmethod
    def _bool_value(value: str) -> bool:
        if value in ('true', 'True', 'yes', 'Yes', '1'):
            return True
        if value in ('false', 'False', 'no', 'No', '0'):
            return False
        raise ValueError(
            'Expected one of: true, yes, 1, false, no, 0 (case-insensitive).')

    @staticmethod
    def _int_list_value(value: str, range_: Optional[list]) -> List[int]:
        if value.startswith('[') and value.endswith(']'):
            value_list_str = value[1:-1]
        else:
            value_list_str = value

        value_list = [item.strip() for item in value_list_str.strip().split(',')]
        if len(value_list) == 0 or (len(value_list) == 1 and value_list[0] == ''):
            raise ValueError('The list of integers is empty.')
        result = []
        for item in value_list:
            result.append(ConfigParser._check_range(int(item), range_))
        return result

    @staticmethod
    def _str_value(value: str) -> str:
        if ((value.startswith('"') and value.endswith('"'))
                or (value.startswith("'") and value.endswith("'"))):
            return value[1:-1]
        return value

    def __getitem__(self, item):
        return self.options[item]

    _get_DEFAULT = object()

    def get(self, key, default=_get_DEFAULT):
        if default == self._get_DEFAULT:
            return self.options.get(key)
        else:
            return self.options.get(key, default)
