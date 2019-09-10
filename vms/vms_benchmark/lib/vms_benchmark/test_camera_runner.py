from os import kill, waitpid
import platform


class TestCameraRunner:
    binary_file = './testcamera'
    test_file_high_resolution = './test_file_high_resolution.ts'
    test_file_low_resolution = './test_file_low_resolution.ts'
    debug = False

    def __init__(self, local_ip, pid, count):
        self.local_ip = local_ip
        self.pid = pid
        self.count = count

    def __del__(self):
        try:
            kill(self.pid, 9)
            waitpid(self.pid, 0)
        except:  #< Probably, the testcamera processes are already finished.
            pass

    @staticmethod
    def spawn(local_ip, lowStreamFps, count=1):
        camera_command = TestCameraRunner.binary_file
        camera_args = [
            TestCameraRunner.binary_file,
            f"--local-interface={local_ip}",
            f"--fps {lowStreamFps}",
            f"files=\"{TestCameraRunner.test_file_high_resolution}\";secondary-files=\"{TestCameraRunner.test_file_low_resolution}\";count={count}",
        ]

        if platform.system() == 'Linux':
            import os
            from os import fork, execvpe, close

            environ = os.environ.copy()
            environ['LD_LIBRARY_PATH'] = os.path.dirname(TestCameraRunner.binary_file)

            pid = fork()

            if pid == 0:
                close(0)
                if not TestCameraRunner.debug:
                    [close(fd) for fd in (1, 2)]
                execvpe(camera_command, camera_args, environ)

        elif platform.system() == 'Windows':
            import subprocess

            opts = {}

            if not TestCameraRunner.debug:
                opts['stdout'] = subprocess.PIPE
                opts['stderr'] = subprocess.PIPE

            proc = subprocess.Popen(camera_args, **opts)
            pid = proc.pid
        else:
            pass

        return TestCameraRunner(local_ip, pid, count)
