import re

from framework.os_access.windows_remoting._cim_query import CIMQuery


class EnvVars(object):
    _env_var_re = re.compile('%(?P<var_name>\w+)%')

    def __init__(self, all_vars, user_name, default_user_env_vars):
        self._resolved_vars = all_vars['<SYSTEM>'].copy()
        self._resolved_vars.update(default_user_env_vars)
        self._resolved_vars.update(all_vars[user_name])

    @classmethod
    def request(cls, pywinrm_protocol, user_name, default_user_env_vars):
        """Return (user name -> (var name -> var value)); values may contain other vars"""
        query = CIMQuery(pywinrm_protocol, 'Win32_Environment', {})
        var_objects = list(query.enumerate())
        vars = {}
        for var_object in var_objects:
            user_name = var_object['UserName']
            user_variables = vars.setdefault(user_name, {})
            name = var_object['Name'].upper()  # Names are case-insensitive.
            value = var_object['VariableValue']
            user_variables[name] = value
        return cls(vars, user_name, default_user_env_vars)

    def _resolution_dfs(self, name, depth):
        # Env vars are resolved on demand.
        # Many env vars are relied to vars which are constructed by Windows dynamically.
        # Those are to be passed via `default_user_env_vars`.
        name = name.upper()

        def replace(match):
            if depth > 10:
                raise RuntimeError("Reached replacement recursion depth limit")
            return self._resolution_dfs(match.group('var_name'), depth + 1)

        old_value = self._resolved_vars[name]
        new_value = self._env_var_re.sub(replace, old_value)
        self._resolved_vars[name] = new_value
        return new_value

    def __getitem__(self, name):
        return self._resolution_dfs(name, 0)
