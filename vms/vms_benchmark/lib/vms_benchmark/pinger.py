import os
import platform
import subprocess
from pathlib import Path

from . import exceptions

class Pinger:
    def __init__(self, host: str):
        if platform.system() == 'Windows':
            _ping_exe = Path(os.environ['WINDIR']).joinpath("System32", "ping.exe")
            if not _ping_exe.exists():
                raise FileNotFoundError(f"{str(_ping_exe)!r} doesn't exist.")

            self._ping_command = [_ping_exe.as_posix(), "-n", "1", host]

        else:
            _ping_exe_std_path_1 = Path("/bin/ping")
            _ping_exe_std_path_2 = Path("/sbin/ping")
            _ping_exe = (_ping_exe_std_path_1
                if _ping_exe_std_path_1.exists()
                else _ping_exe_std_path_2)

            if not _ping_exe.exists():
                raise FileNotFoundError(
                    f"neither {str(_ping_exe_std_path_1)!r} "
                    f"nor {str(_ping_exe_std_path_2)!r} exist.")

            self._ping_command = [_ping_exe.as_posix(), "-c1", host]
        
    
    def ping(self) -> bool:
        completed_proc = subprocess.run(
            self._ping_command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
        return completed_proc.returncode == 0
