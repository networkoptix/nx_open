from textwrap import dedent

from netaddr import EUI, IPAddress
from pathlib2 import PurePath


def sh_quote_arg(arg):
    try:
        # noinspection PyProtectedMember
        from shlex import quote  # In Python 3.3+.
    except ImportError:
        # noinspection PyProtectedMember
        from pipes import quote  # In Python 2.7 but deprecated.

    return quote(str(arg))


def sh_command_to_script(command):
    return ' '.join(sh_quote_arg(str(arg)) for arg in command)


def sh_convert_env_values_to_str(env):
    converted_env = {}
    for name, value in env.items():
        if isinstance(value, bool):  # Beware: bool is subclass of int.
            converted_env[name] = 'true' if value else 'false'
            continue
        if isinstance(value, (int, PurePath, IPAddress, EUI)):
            converted_env[name] = str(value)
            continue
        if isinstance(value, str):
            converted_env[name] = value
            continue
        if value is None:
            converted_env[name] = ''
            continue
        raise RuntimeError("Unexpected value {!r} of type {}".format(value, type(value)))
    return converted_env


_SH_PROHIBITED_ENV_NAMES = {'PATH', 'HOME', 'USER', 'SHELL', 'PWD', 'TERM'}


def sh_env_to_command(env):
    converted_env = sh_convert_env_values_to_str(env)
    command = []
    for name, value in converted_env.items():
        if name in _SH_PROHIBITED_ENV_NAMES:
            raise ValueError("Potential name clash with built-in name: {}".format(name))
        command.append('{}={}'.format(name, sh_quote_arg(str(value))))
    return command


# language=Bash
def sh_augment_script(script, cwd=None, env=None, set_eux=True, shebang=False):
    augmented_script_lines = []
    if shebang:
        augmented_script_lines.append('#!/bin/sh')
    if set_eux:
        augmented_script_lines.append('set -eux')  # It's sh (dash), pipefail cannot be set here.
    if cwd is not None:
        augmented_script_lines.append(sh_command_to_script(['cd', cwd]))
    if env is not None:
        augmented_script_lines.extend(sh_env_to_command(env))
    augmented_script_lines.append(dedent(script).strip())
    augmented_script = '\n'.join(augmented_script_lines)
    return augmented_script
