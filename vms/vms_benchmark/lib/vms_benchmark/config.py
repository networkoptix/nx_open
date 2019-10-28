import typing

from vms_benchmark import exceptions


class NoConfigFile(exceptions.VmsBenchmarkError):
    pass


class InvalidConfigFileContent(exceptions.VmsBenchmarkError):
    pass


class ConfigOptionNotFound(exceptions.VmsBenchmarkError):
    pass


class InvalidConfigOption(exceptions.VmsBenchmarkError):
    pass


class ConfigOptionValueError(exceptions.VmsBenchmarkError):
    def __init__(self, conf_file, name, value, original_exception=None):
        super(ConfigOptionValueError, self).__init__(
            f"Error in file {conf_file}: Invalid value {repr(value)} of option '{name}'."
        )
        self.original_exception = original_exception


class ConfigParser:
    @staticmethod
    def _process_env_vars(string):
        import re
        from os import environ
        return re.sub(r"\$([a-zA-Z_][a-zA-Z_]+)",
            lambda match: environ.get(match.group(1), ''), string)

    @staticmethod
    def _load_file(filename, is_file_optional) -> typing.Optional[dict]:
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

        options = ConfigParser._load_file(filename, is_file_optional)
        if options is None:
            self.options = dict()
            self.OPTIONS_FROM_FILE = None
        else:
            self.options = options
            self.OPTIONS_FROM_FILE = self.options.copy()

        if definition:
            for name, _ in (
                    (k, v) for (k, v) in definition.items() if v.get('optional', False) is False):
                if name not in self.options.keys():
                    raise ConfigOptionNotFound(
                        f"Mandatory option '{name}' is not defined in {filename!r}.")

            for name, option_def in definition.items():
                try:
                    self._process_option(filename, name, option_def)
                except InvalidConfigOption:
                    raise
                except Exception as e:
                    raise ConfigOptionValueError(filename, name, self.options[name], e)

            for name, value in self.options.items():
                if name not in definition.keys():
                    raise InvalidConfigOption(f"Unexpected option '{name}' in {filename!r}.")

    def _process_option(self, filename, name, option_def: dict):
        if 'default' in option_def:
            self.options[name] = self.options.get(
                name, option_def['default'])

        if name not in self.options:
            return

        if option_def['type'] == "int":
            self.options[name] = int(self.options[name])
        elif option_def['type'] == "float":
            self.options[name] = float(self.options[name])
        elif option_def['type'] == "bool":
            if self.options[name] in (True, 'true', 'True', 'yes', 'Yes', '1'):
                value = True
            elif self.options[name] in (False, 'false', 'False', 'no', 'No', '0'):
                value = False
            else:
                raise Exception(
                    'Expected one of: true, yes, 1, false, no, 0 (case-insensitive).')
            self.options[name] = value
        elif option_def['type'] == 'intList':
            value = self.options[name]
            if not isinstance(value, list):
                # Parse integer list from string into list. Brackets are optional.
                if value.startswith('[') and value.endswith(']'):
                    value_list_str = value[1:-1]
                else:
                    value_list_str = value
                value_list = (
                    [item.strip() for item in value_list_str.strip().split(',')])
                if len(value_list) == 0 or (
                    len(value_list) == 1 and value_list[0] == ''):
                    raise Exception('List of integers is empty.')
                self.options[name] = [int(item.strip()) for item in value_list]
        elif option_def['type'] == "str":
            value = self.options[name]
            if ((value.startswith('"') and value.endswith('"'))
                or (value.startswith("'") and value.endswith("'"))):
                value = value[1:-1]
            self.options[name] = value
            if not ('default' in option_def) and not value:
                raise InvalidConfigOption(
                    f"Empty value is not allowed for option '{name}' in {filename!r}.")
        else:
            raise InvalidConfigOption(
                f"Unexpected type {option_def['type']!r} "
                f"in '{name}' option "
                f"in the definition for {filename!r}.")

    def __getitem__(self, item):
        return self.options[item]

    _get_DEFAULT = object()

    def get(self, key, default=_get_DEFAULT):
        if default == self._get_DEFAULT:
            return self.options.get(key)
        else:
            return self.options.get(key, default)
