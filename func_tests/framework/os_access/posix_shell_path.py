from abc import ABCMeta, abstractproperty
from functools import wraps

from pathlib2 import PurePosixPath

from framework.os_access import exceptions
from framework.os_access.path import FileSystemPath
from framework.os_access.posix_shell import Shell


def _raising_on_exit_status(exit_status_to_error_cls):
    def decorator(func):
        @wraps(func)
        def decorated(self, *args, **kwargs):
            try:
                return func(self, *args, **kwargs)
            except exceptions.NonZeroExitStatus as e:
                if e.exit_status in exit_status_to_error_cls:
                    error_cls = exit_status_to_error_cls[e.exit_status]
                    raise error_cls(e)
                raise

        return decorated

    return decorator


class PosixShellPath(FileSystemPath, PurePosixPath):
    """Base class for file system access through SSH

    It's the simplest way to integrate with `pathlib` and `pathlib2`.
    When manipulating with paths, `pathlib` doesn't call neither
    `__new__` nor `__init__`. The only information preserved is type.
    That's why `PosixShell` instance is bound to class, not to object.
    This class is not on a module level, therefore,
    it will be referenced by `SSHAccess` object and by path objects
    and will live until those objects live.
    """
    __metaclass__ = ABCMeta
    _shell = abstractproperty()  # type: Shell # PurePath's manipulations can preserve only the type.

    @classmethod
    def specific_cls(cls, posix_shell):
        class SpecificPosixShellPath(cls):
            _shell = posix_shell

        return SpecificPosixShellPath

    @classmethod
    def home(cls):
        # Returning `echo ~` output doesn't work since tests may me run in environment with no
        # `$HOME` env var, e.g. under `tox`, where `~` is not expanded by shell.
        return cls('~').expanduser()

    @classmethod
    def tmp(cls):
        return cls('/tmp/func_tests')

    def __repr__(self):
        return '<PosixShellPath {!s} on {!r}>'.format(self, self._shell)

    def exists(self):
        try:
            self._shell.run_command(['test', '-e', self])
        except exceptions.exit_status_error_cls(1):
            return False
        else:
            return True

    @_raising_on_exit_status({2: exceptions.DoesNotExist, 3: exceptions.NotAFile})
    def unlink(self):
        self._shell.run_sh_script(
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
            user_name = self._shell.command(['whoami']).run().rstrip('\n')
        else:
            user_name = self.parts[0][1:]
        output = self._shell.run_command(['getent', 'passwd', user_name])
        if not output:
            raise RuntimeError("Can't determine home directory for {!r}".format(user_name))
        user_home_dir = output.split(':')[5]
        return self.__class__(user_home_dir, *self.parts[1:])

    @_raising_on_exit_status({2: exceptions.DoesNotExist, 3: exceptions.NotADir})
    def glob(self, pattern):
        output = self._shell.run_sh_script(
            # language=Bash
            '''
                test ! -e "$SELF" && >&2 echo "does not exist: $SELF" && exit 2
                test ! -d "$SELF" && >&2 echo "not a dir: $SELF" && exit 3
                find "$SELF" -mindepth 1 -maxdepth 1 -name "$PATTERN"
                ''',
            env={'SELF': self, 'PATTERN': pattern})
        lines = output.decode('ascii').rstrip().splitlines()
        paths = [self.__class__(line) for line in lines]
        return paths

    @_raising_on_exit_status({2: exceptions.BadParent, 3: exceptions.AlreadyExists, 4: exceptions.BadParent})
    def mkdir(self, parents=False, exist_ok=False):
        self._shell.run_sh_script(
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
            self._shell.run_sh_script(
                # language=Bash
                '''
                    test -e "$SELF" || exit 2
                    test -d "$SELF" || exit 3
                    rm -r -- "$SELF"
                    ''',
                env={'SELF': self})
        except exceptions.exit_status_error_cls(2):
            if not ignore_errors:
                raise exceptions.DoesNotExist(self)
        except exceptions.exit_status_error_cls(3):
            if not ignore_errors:
                raise exceptions.NotADir(self)

    @_raising_on_exit_status({2: exceptions.DoesNotExist, 3: exceptions.NotAFile})
    def read_bytes(self, offset=None, max_length=None):
        return self._shell.run_sh_script(
            # language=Bash
            '''
                test ! -e "$SELF" && >&2 echo "does not exist: $SELF" && exit 2
                test ! -f "$SELF" && >&2 echo "not a file: $SELF" && exit 3
                if [ -z $OFFSET ]; then
                    cat "$SELF"
                else
                    dd bs=$((1024 * 1024)) if="$SELF" skip=$OFFSET count=$MAX_LENGTH iflag=skip_bytes,count_bytes
                fi
                ''',
            env={'SELF': self, 'OFFSET': offset, 'MAX_LENGTH': max_length},
            timeout_sec=600,
            )

    @_raising_on_exit_status({2: exceptions.BadParent, 3: exceptions.BadParent, 4: exceptions.NotAFile})
    def write_bytes(self, contents, offset=None):
        output = self._shell.run_sh_script(
            # language=Bash
            '''
                parent="$(dirname "$SELF")"
                test ! -e "$parent" && >&2 echo "no parent: $parent" && exit 2
                test ! -d "$parent" && >&2 echo "parent not a dir: $parent" && exit 3
                test -e "$SELF" -a ! -f "$SELF" && >&2 echo "not a file: $SELF" && exit 4
                if [ -z $OFFSET ]; then
                    cat >"$SELF"
                    stat --printf="%s" "$SELF"
                else
                    dd bs=$((1024 * 1024)) conv=notrunc of="$SELF" seek=$OFFSET oflag=seek_bytes
                fi
                ''',
            env={'SELF': self, 'OFFSET': offset},
            input=contents,
            timeout_sec=600,
            )
        written = int(output) if output else len(contents)
        return written

    def read_text(self, encoding='ascii', errors='strict'):
        # ASCII encoding is single used encoding in the project.
        return self.read_bytes().decode(encoding=encoding, errors=errors)

    def write_text(self, text, encoding='ascii', errors='strict'):
        # ASCII encoding is single used encoding in the project.
        data = text.encode(encoding=encoding, errors=errors)
        bytes_written = self.write_bytes(data)
        assert bytes_written == len(data)
        return len(text)

    def copy_to(self, destination):
        self._shell.copy_posix_file_to(self, destination)

    def copy_from(self, source):
        self._shell.copy_file_from_posix(source, self)
