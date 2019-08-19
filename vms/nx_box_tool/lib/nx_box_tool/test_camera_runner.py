from os import close, environ, fork, execvpe, kill, waitpid


class TestCameraRunner:
    def __init__(self, local_ip, pid, count):
        self.local_ip = local_ip
        self.pid = pid
        self.count = count

    def __del__(self):
        kill(self.pid, 9)
        waitpid(self.pid, 0)

    @staticmethod
    def spawn(local_ip, count=1):
        camera_command = './testcamera'
        camera_args = [
            './testcamera',
            f"--local-interface={local_ip}",
            f"files=\"./la-traffic-speeding-towards.ts\";count={count}"
        ]

        pid = fork()

        if pid == 0:
            close(0)
            close(1)
            close(2)
            execvpe(camera_command, camera_args, environ)

        return TestCameraRunner(local_ip, pid, count)
