from pathlib2 import PurePosixPath


class RemoteDaemon(object):
    def __init__(self, name, args, env=None):
        """Leave daemon running and stop it at next execution only."""
        self.args = args
        self.stdout_file = PurePosixPath('/tmp', name + '.stdout.txt')
        self.stderr_file = PurePosixPath('/tmp', name + '.stderr.txt')
        self.pid_file = PurePosixPath('/run', name + '.pid')
        self.env = env

    def status(self, os_access):
        stdout = os_access.run_command([
            'if', 'test', '-e', self.pid_file, ';', 'then',
            'ps', '-p', '$(cat {})'.format(self.pid_file), '-o', 'command=', '|', 'wc', '-l', ';',
            'else',
            'echo', '0', ';',
            'fi',
            ])
        if stdout.strip() == b'0':
            return 'stopped'
        if stdout.strip() == b'1':
            return 'started'
        assert False, "Expected '0' or '1' in stdout but got:\n%s" % stdout

    def start(self, os_access):
        redirect_args = ['>', self.stdout_file, '2>', self.stderr_file]
        pid_file_args = ['echo', '$!', '>', self.pid_file]
        # Correct command which is executed on target (usually remote) machine is:
        # nohup serve -a -b & echo $! > /run/serve.pid
        # Single ampersand delimits next command as well as makes command a job.
        os_access.run_command(['nohup'] + self.args + redirect_args + ['&'] + pid_file_args, env=self.env)

    def stop(self, os_access):
        os_access.run_command(['pkill', '-F', self.pid_file])


watch_date = RemoteDaemon('watch-date', ['watch', '-n', '0.1', 'date', '--iso-8601', 'ns'])
