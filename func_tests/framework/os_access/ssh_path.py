from abc import ABCMeta

from pathlib2 import PurePosixPath

from framework.os_access.exceptions import FileNotFound, exit_status_error_cls
from framework.os_access.local_access import LocalAccess
from framework.os_access.path import FileSystemPath
from framework.os_access.args import sh_command_to_script, sh_quote_arg
from framework.os_access.ssh_access import SSHAccess


class SSHPath(FileSystemPath, PurePosixPath):
    __metaclass__ = ABCMeta

    _ssh_access = None  # type: SSHAccess

    @classmethod
    def home(cls):
        return cls(cls._ssh_access.run_sh_script('echo ~').rstrip())

    @classmethod
    def tmp(cls):
        return cls('/tmp/func_tests')

    def touch(self, mode=0o666, exist_ok=True):
        self._ssh_access.run_sh_script(
            # language=Bash
            '''
                if [[ -e $FILE_PATH ]]; then
                    echo "$FILE_PATH already exists" >&2
                    if [[ $EXIST_OK == false ]]; then
                        exit 2
                    fi
                else
                    touch "$FILE_PATH"
                    chmod $MODE "$FILE_PATH"  # Change permissions only if created as in pathlib2.
                fi
                ''',
            env={
                'FILE_PATH': self,  # Don't overwrite $PATH!
                'EXIST_OK': 'true' if exist_ok else 'false',
                'MODE': oct(mode),
                })

    def exists(self):
        try:
            self._ssh_access.run_command(['test', '-e', self])
        except exit_status_error_cls(1):
            return False
        else:
            return True

    def unlink(self):
        self._ssh_access.run_command(['rm', '-v', '--', self])

    def iterdir(self):
        return self._run_find_command(max_depth=1)

    def expanduser(self):
        """Expand tilde at the beginning safely (without passing complete path to sh)"""
        if not self.parts[0].startswith('~'):
            return self
        if self.parts[0] == '~':
            return self.home().joinpath(*self.parts[1:])
        user_name = self.parts[0][1:]
        output = self._ssh_access.run_command(['getent', 'passwd', user_name])
        if not output:
            raise RuntimeError("Can't determine home directory for {!r}".format(user_name))
        user_home_dir = output.split(':')[6]
        return self.__class__(user_home_dir, *self.parts[1:])

    def _run_find_command(self, whole_name_pattern=None, max_depth=None):
        command = ['find', self]
        if max_depth is not None:
            command += ['-maxdepth', max_depth]
        if whole_name_pattern is not None:
            command += ['-wholename', whole_name_pattern]
        output = self._ssh_access.run_command(command)
        lines = output.rstrip().splitlines()
        return [self.__class__(line) for line in lines]

    def glob(self, pattern):
        return self._run_find_command(whole_name_pattern=self / pattern, max_depth=1)

    def rglob(self, pattern):
        return self._run_find_command(whole_name_pattern=self / pattern)

    def mkdir(self, parents=False, exist_ok=False):
        self._ssh_access.run_sh_script(
            # language=Bash
            '''
                if [ -e "$DIR" ]; then
                    if [ -d "$DIR" ]; then
                        >&2 echo "$DIR already exists"
                        if [ "$EXIST_OK" == true ]; then
                            exit 0
                        else
                            exit 2
                        fi
                    else
                        >&2 echo "$DIR already exists but not a dir" && exit 3
                    fi
                else
                    if [ "$PARENTS" == true ]; then
                        mkdir -vp -- "$DIR"
                    else
                        mkdir -v -- "$DIR"
                    fi
                fi
                ''',
            env={'DIR': self, 'PARENTS': parents, 'EXIST_OK': exist_ok})

    def rmtree(self, ignore_errors=False):
        self._ssh_access.run_command(['rm', '-v', '-rf' if ignore_errors else '-r', '--', self])

    def read_bytes(self):
        try:
            return self._ssh_access.run_command(['cat', self])
        except exit_status_error_cls(1):
            raise FileNotFound(self)

    def write_bytes(self, contents):
        self._ssh_access.run_sh_script('cat > {}'.format(sh_quote_arg(self)), input=contents)

    def read_text(self, encoding='ascii', errors='strict'):
        # ASCII encoding is single used encoding in the project.
        return self.read_bytes().decode(encoding=encoding, errors=errors)

    def write_text(self, data, encoding='ascii', errors='strict'):
        # ASCII encoding is single used encoding in the project.
        self.write_bytes(data.encode(encoding=encoding, errors=errors))

    def _call_rsync(self, source, destination):
        # If file size and modification time is same, rsync doesn't copy file.
        LocalAccess().run_command([
            'scp',
            '-C',  # Compression.
            '-F', self._ssh_access.config_path,
            '-P', self._ssh_access.port,
            source, destination,
            ])

    def upload(self, local_source_path):
        remote_destination_path = '{}:{}'.format(self._ssh_access.hostname, self)
        try:
            self._ssh_access.run_command(['test', '-d', self])
        except exit_status_error_cls(1):
            self._call_rsync(local_source_path, remote_destination_path)
        else:
            raise RuntimeError("Destination is a dir; had command run, file would've been placed into this dir")

    def download(self, local_destination_path):
        remote_source_path = '{}:{}'.format(self._ssh_access.hostname, self)
        self._call_rsync(remote_source_path, local_destination_path)
