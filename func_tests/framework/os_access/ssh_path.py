from abc import ABCMeta, abstractproperty
from functools import wraps

from pathlib2 import PurePosixPath

from framework.os_access.exceptions import (
    AlreadyExists,
    BadParent,
    DoesNotExist,
    NonZeroExitStatus,
    NotADir,
    NotAFile,
    exit_status_error_cls,
    )
from framework.os_access.path import FileSystemPath


def _raising_on_exit_status(exit_status_to_error_cls):
    def decorator(func):
        @wraps(func)
        def decorated(self, *args, **kwargs):
            try:
                return func(self, *args, **kwargs)
            except NonZeroExitStatus as e:
                if e.exit_status in exit_status_to_error_cls:
                    error_cls = exit_status_to_error_cls[e.exit_status]
                    raise error_cls(e)
                raise

        return decorated

    return decorator


class SSHPath(FileSystemPath, PurePosixPath):
    __metaclass__ = ABCMeta
    _ssh = abstractproperty()  # PurePath's manipulations can preserve only the type.

    @classmethod
    def home(cls):
        return cls(cls._ssh.run_sh_script('echo ~').rstrip())

    @classmethod
    def tmp(cls):
        return cls('/tmp/func_tests')

    def exists(self):
        try:
            self._ssh.run_command(['test', '-e', self])
        except exit_status_error_cls(1):
            return False
        else:
            return True

    @_raising_on_exit_status({2: DoesNotExist, 3: NotAFile})
    def unlink(self):
        self._ssh.run_sh_script(
            # language=Bash
            '''
                test ! -e "$SELF" && >&2 echo "does not exist: $SELF" && exit 2
                test ! -f "$SELF" && >&2 echo "not a file: $SELF" && exit 3
                rm -v -- "$SELF"
                ''',
            env={'SELF': self})

    def expanduser(self):
        """Expand tilde at the beginning safely (without passing complete path to sh)"""
        if not self.parts[0].startswith('~'):
            return self
        if self.parts[0] == '~':
            return self.home().joinpath(*self.parts[1:])
        user_name = self.parts[0][1:]
        output = self._ssh.run_command(['getent', 'passwd', user_name])
        if not output:
            raise RuntimeError("Can't determine home directory for {!r}".format(user_name))
        user_home_dir = output.split(':')[6]
        return self.__class__(user_home_dir, *self.parts[1:])

    @_raising_on_exit_status({2: DoesNotExist, 3: NotADir})
    def glob(self, pattern):
        output = self._ssh.run_sh_script(
            # language=Bash
            '''
                test ! -e "$SELF" && >&2 echo "does not exist: $SELF" && exit 2
                test ! -d "$SELF" && >&2 echo "not a dir: $SELF" && exit 3
                find "$SELF" -mindepth 1 -maxdepth 1 -name "$PATTERN"
                ''',
            env={'SELF': self, 'PATTERN': pattern})
        lines = output.rstrip().splitlines()
        paths = [self.__class__(line) for line in lines]
        return paths

    @_raising_on_exit_status({2: BadParent, 3: AlreadyExists, 4: BadParent})
    def mkdir(self, parents=False, exist_ok=False):
        self._ssh.run_sh_script(
            # language=Bash
            '''
                ancestor="$DIR"
                while [ ! -e "$ancestor" ]; do
                    ancestor="$(dirname "$ancestor")"
                done
                test ! -d "$ancestor" && >&2 echo "not a dir: $ancestor" && exit 2
                test "$ancestor" = "$DIR" -a $EXIST_OK = true && exit 0 
                test "$ancestor" = "$DIR" && >&2 echo "dir exists: $DIR" && exit 3
                if [ "$ancestor" = "$(dirname "$DIR")" ]; then
                    mkdir -v -- "$DIR"
                else
                    if [ $PARENTS = true ]; then
                        mkdir -vp -- "$DIR"
                    else 
                        >&2 echo "does not exist: $(dirname "$DIR")"
                        exit 4
                    fi
                fi
                ''',
            env={'DIR': self, 'PARENTS': parents, 'EXIST_OK': exist_ok})

    def rmtree(self, ignore_errors=False):
        try:
            self._ssh.run_sh_script(
                # language=Bash
                '''
                    test -e "$SELF" || exit 2
                    test -d "$SELF" || exit 3
                    rm -r -- "$SELF"
                    ''',
                env={'SELF': self})
        except exit_status_error_cls(2):
            if not ignore_errors:
                raise DoesNotExist(self)
        except exit_status_error_cls(3):
            if not ignore_errors:
                raise NotADir(self)

    @_raising_on_exit_status({2: DoesNotExist, 3: NotAFile})
    def read_bytes(self):
        return self._ssh.run_sh_script(
            # language=Bash
            '''
                test ! -e "$SELF" && >&2 echo "does not exist: $SELF" && exit 2
                test ! -f "$SELF" && >&2 echo "not a file: $SELF" && exit 3
                cat "$SELF"
                ''',
            env={'SELF': self})

    @_raising_on_exit_status({2: BadParent, 3: BadParent, 4: NotAFile})
    def write_bytes(self, contents):
        output = self._ssh.run_sh_script(
            # language=Bash
            '''
                parent="$(dirname "$SELF")"
                test ! -e "$parent" && >&2 echo "no parent: $parent" && exit 2
                test ! -d "$parent" && >&2 echo "parent not a dir: $parent" && exit 3
                test -e "$SELF" -a ! -f "$SELF" && >&2 echo "not a file: $SELF" && exit 4
                cat >"$SELF"
                stat --printf="%s" "$SELF"
                ''',
            env={'SELF': self},
            input=contents)
        written = int(output)
        return written

    def read_text(self, encoding='ascii', errors='strict'):
        # ASCII encoding is single used encoding in the project.
        return self.read_bytes().decode(encoding=encoding, errors=errors)

    def write_text(self, data, encoding='ascii', errors='strict'):
        # ASCII encoding is single used encoding in the project.
        self.write_bytes(data.encode(encoding=encoding, errors=errors))


def make_ssh_path_cls(ssh):
    """Separate function to be used within SSHAccess and with ad-hoc SSH"""
    class SpecificSSHPath(SSHPath):
        _ssh = ssh

    return SpecificSSHPath
