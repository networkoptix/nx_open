try:
    from shlex import quote  # In Python 3.3+.
except ImportError:
    from pipes import quote  # In Python 2.7 but deprecated.


def args_to_command(args):
    if isinstance(args, str):
        return args
    return ' '.join(quote(str(arg)) for arg in args)


def env_to_args(env):
    env = env or dict()
    return ['{}={}'.format(name, quote(str(value))) for name, value in env.items()]
