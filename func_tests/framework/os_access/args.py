from subprocess import list2cmdline
from textwrap import dedent

from netaddr import EUI, IPAddress
from pathlib2 import PurePath


def cmd_command_to_script(command):
    return list2cmdline(str(arg) for arg in command)


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
        raise RuntimeError("Unexpected value {!r} of type {}".format(value, type(value)))
    return converted_env


_SH_PROHIBITED_ENV_NAMES = {'PATH', 'HOME', 'USER', 'SHELL', 'PWD', 'TERM'}


def sh_env_to_command(env):
    converted_env = sh_convert_env_values_to_str(env)
    command = []
    for name, value in converted_env.items():
        if name in _SH_PROHIBITED_ENV_NAMES:
            raise ValueError("")
        command.append('{}={}'.format(name, sh_quote_arg(str(value))))
    return command


def sh_augment_script(script, cwd, env):
    script = dedent(script).strip()
    if cwd is not None:
        script = sh_command_to_script(['cd', cwd]) + '\n' + script
    if env is not None:
        script = '\n'.join(sh_env_to_command(env)) + '\n' + script
    script = 'set -eux' + '\n' + script
    return script
