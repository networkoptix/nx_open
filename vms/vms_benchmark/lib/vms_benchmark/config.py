from vms_benchmark import exceptions


class NoConfigFile(exceptions.VmsBenchmarkError):
    pass


class ConfigOptionNotFound(exceptions.VmsBenchmarkError):
    pass


class InvalidConfigOption(exceptions.VmsBenchmarkError):
    pass


class ConfigOptionValueError(exceptions.VmsBenchmarkError):
    def __init__(self, conf_file, name, value, original_exception=None):
        super(ConfigOptionValueError, self).__init__(
            f"Error in file {conf_file}: Invalid value '{value}' of option '{name}'."
        )
        self.original_exception = original_exception


class ConfigParser:
    def __init__(self, filepath, option_descriptions=None, is_file_optional=False):
        assert option_descriptions or not is_file_optional

        is_file_present = True
        try:
            f = open(filepath)
        except FileNotFoundError:
            if is_file_optional:
                is_file_present = False
            else:
                raise NoConfigFile(f"Config file '{filepath}' not found.")

        def process_env_vars(string):
            import re
            from os import environ
            return re.sub(r"\$([a-zA-Z_][a-zA-Z_]+)", lambda match: environ.get(match.group(1), ''), string)

        if not is_file_present:
            self.options = dict()
        else:
            self.options = dict(
                (
                    line[:line.find('=')].strip(),
                    process_env_vars(line[line.find('=') + 1:].strip())
                ) for line in (
                    line
                    for line in [
                        line[:line.find('#')] if line.find('#') != -1 else line
                        for line in
                        f.readlines()
                    ]
                    if '=' in line
                )
            )

        self.ORIGINAL_OPTIONS = self.options.copy() if is_file_present else None

        if option_descriptions:
            for name, _ in ((k, v) for (k, v) in option_descriptions.items() if v.get('optional', False) is False):
                if name not in self.options.keys():
                    raise ConfigOptionNotFound(f"Mandatory option '{name}' is not defined in {filepath}.")

            for name, _ in option_descriptions.items():
                try:
                    if 'default' in option_descriptions[name]:
                        self.options[name] = self.options.get(name, option_descriptions[name]['default'])

                    if option_descriptions[name]['type'] == 'integer' and name in self.options:
                        value = int(self.options[name])
                        self.options[name] = value
                    if option_descriptions[name]['type'] == 'float' and name in self.options:
                        self.options[name] = float(self.options[name])
                    if option_descriptions[name]['type'] == 'boolean' and name in self.options:
                        if self.options[name] in ('true', 'True', 'yes', 'Yes', '1'):
                            value = True
                        elif self.options[name] in ('false', 'False', 'no', 'No', '0'):
                            value = False
                        else:
                            raise Exception(
                                'Expected one of: true, True, yes, Yes, 1, false, False, no, No, 0.')
                        self.options[name] = value
                    elif option_descriptions[name]['type'] == 'integers' and name in self.options:
                        # Parse integers from string into list. Brackets are optional.
                        value = self.options[name]
                        value_list_str = \
                            value[1:-1] if value.startswith('[') and value.endswith(']') else value
                        value_list = [item.strip() for item in value_list_str.strip().split(',')]
                        if len(value_list) == 0 or (len(value_list) == 1 and value_list[0] == ''):
                            raise Exception('List of integers is empty.')
                        self.options[name] = [int(item.strip()) for item in value_list]
                except Exception as e:
                    raise ConfigOptionValueError(filepath, name, self.options[name], e)

            for name, value in self.options.items():
                if name not in option_descriptions.keys():
                    raise InvalidConfigOption(f"Unexpected option '{name}' in {filepath}.")

    def __getitem__(self, item):
        return self.options[item]

    _get_DEFAULT = object()

    def get(self, key, default=_get_DEFAULT):
        if default == self._get_DEFAULT:
            return self.options.get(key)
        else:
            return self.options.get(key, default)
