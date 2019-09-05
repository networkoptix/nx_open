from nx_box_tool import exceptions


class NoConfigFile(exceptions.NxBoxToolError):
    pass


class ConfigOptionNotFound(exceptions.NxBoxToolError):
    pass


class InvalidConfigOption(exceptions.NxBoxToolError):
    pass


class ConfigParser:
    def __init__(self, filepath, option_descriptions=None):
        try:
            f = open(filepath)
        except FileNotFoundError:
            raise NoConfigFile(f"Config file '{filepath}' not found.")

        self.options = dict([
            [
                line[:line.find('=')].strip(),
                line[line.find('=') + 1:].strip()
            ] for line in [
                line for line in [
                    line[:line.find('#')] if line.find('#') != -1 else line
                    for line in
                    f.readlines()
                ]
                if '=' in line
            ]
        ])

        if option_descriptions:
            for name, _ in ((k, v) for (k, v) in option_descriptions.items() if v.get('optional', False) is False):
                if name not in self.options.keys():
                    raise ConfigOptionNotFound(f"Mandatory config option '{name}' is not defined in config {filepath}.")

            for name, _ in option_descriptions.items():
                if 'default' in option_descriptions[name]:
                    self.options[name] = self.options.get(name, option_descriptions[name]['default'])

                if option_descriptions[name]['type'] == 'integer':
                    self.options[name] = int(self.options[name])
                if option_descriptions[name]['type'] == 'boolean':
                    self.options[name] = self.options[name] in ('true', 'True', 't', 'yes', 'Yes', '1')
                elif option_descriptions[name]['type'] == 'integers' and self.options.get(name, None) is not None:
                    self.options[name] = [int(item.strip()) for item in self.options[name].split(',')]
                elif option_descriptions[name]['type'] == 'strings' and self.options.get(name, None) is not None:
                    self.options[name] = [item.strip() for item in self.options[name].split(',')]

            for name, value in self.options.items():
                if name not in option_descriptions.keys():
                    raise InvalidConfigOption(f"Unexpected option '{name}' in config {filepath}.")

    def __getitem__(self, item):
        return self.options[item]

    _get_DEFAULT=object()

    def get(self, key, default=_get_DEFAULT):
        if default == self._get_DEFAULT:
            return self.options.get(key)
        else:
            return self.options.get(key, default)
