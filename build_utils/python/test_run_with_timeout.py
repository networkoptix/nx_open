#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import re
import subprocess
import sys
import tempfile
import textwrap
import time
import unittest
import uuid
from pathlib import Path

SCRIPT = Path(__file__).with_name('run_with_timeout.py')


# Holds the file given as its argument open, which keeps it from being deleted on Windows.
HOLD_FILE_CODE = 'import sys, time; f = open(sys.argv[1]); time.sleep(606)'

# Puts itself into a job that does not allow breaking away, then runs its arguments.
JOB_LAUNCHER_CODE = textwrap.dedent('''
    import ctypes, subprocess, sys
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.CreateJobObjectW.restype = ctypes.c_void_p
    kernel32.GetCurrentProcess.restype = ctypes.c_void_p
    kernel32.AssignProcessToJobObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    job = kernel32.CreateJobObjectW(None, None)
    if not kernel32.AssignProcessToJobObject(job, kernel32.GetCurrentProcess()):
        sys.exit(f'Unable to join the job: {ctypes.get_last_error()}')
    sys.exit(subprocess.run(sys.argv[1:]).returncode)
    ''')


def run_wrapper(
        child_code: str, timeout: float, wrapper_args=(), child_args=(), launcher=()):
    command = [
        *launcher, sys.executable, str(SCRIPT), '--timeout', str(timeout), *wrapper_args, '--',
        sys.executable, '-c', textwrap.dedent(child_code), *child_args]
    start = time.monotonic()
    result = subprocess.run(command, capture_output=True, timeout=120)
    return result, time.monotonic() - start


class RunWithTimeoutTest(unittest.TestCase):
    def assert_deletable(self, path: Path):
        deadline = time.monotonic() + 10
        while True:
            try:
                path.unlink()
                return
            except PermissionError:
                if time.monotonic() > deadline:
                    self.fail(f'{path} is still held open')
                time.sleep(0.2)

    def test_passes_output_and_exit_code(self):
        result, _ = run_wrapper('''
            import sys
            print('hello from child')
            print('to stderr', file=sys.stderr)
            sys.exit(3)
            ''', timeout=60)
        self.assertEqual(result.returncode, 3)
        self.assertIn(b'hello from child', result.stdout)
        self.assertIn(b'to stderr', result.stdout)

    def test_kills_hung_command_and_reports_it(self):
        result, elapsed = run_wrapper('''
            import time
            print('started', flush=True)
            time.sleep(600)
            ''', timeout=3)
        self.assertNotEqual(result.returncode, 0)
        self.assertLess(elapsed, 60)
        self.assertIn(b'started', result.stdout)
        self.assertIn(b'did not finish in 3 seconds', result.stderr)
        self.assertIn(b'time.sleep(600)', result.stderr)

    def test_kills_hung_grandchild(self):
        result, elapsed = run_wrapper('''
            import subprocess, sys
            subprocess.run([sys.executable, '-c', 'import time; time.sleep(601)'])
            ''', timeout=3)
        self.assertNotEqual(result.returncode, 0)
        self.assertLess(elapsed, 60)
        self.assertIn(b'time.sleep(601)', result.stderr)

    def test_does_not_wait_for_descendant_holding_output_pipe(self):
        result, elapsed = run_wrapper('''
            import subprocess, sys
            subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)'])
            print('main work done', flush=True)
            ''', timeout=600)
        self.assertEqual(result.returncode, 0)
        self.assertLess(elapsed, 25)
        self.assertIn(b'main work done', result.stdout)
        self.assertIn(
            b'output is still held open by these processes, which are killed:', result.stderr)
        self.assertIn(b'time.sleep(30)', result.stderr)

    @unittest.skipUnless(sys.platform == 'win32', 'Open files can be deleted on POSIX')
    def test_kills_descendants_left_after_success(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            held = Path(directory) / 'held'
            held.write_bytes(b'')
            result, _ = run_wrapper(f'''
                import subprocess, sys
                subprocess.Popen([sys.executable, '-c', {HOLD_FILE_CODE!r}, sys.argv[1]])
                ''', timeout=600, child_args=[str(held)])
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertIn(b'time.sleep(606)', result.stderr)
            self.assert_deletable(held)

    @unittest.skipUnless(sys.platform == 'win32', 'Open files can be deleted on POSIX')
    def test_kills_orphan_of_exited_parent(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            held = Path(directory) / 'held'
            held.write_bytes(b'')
            starter_code = (
                'import subprocess, sys; '
                f'subprocess.Popen([sys.executable, "-c", {HOLD_FILE_CODE!r}, sys.argv[1]])')
            result, _ = run_wrapper(f'''
                import subprocess, sys, time
                subprocess.run([sys.executable, '-c', {starter_code!r}, sys.argv[1]])
                time.sleep(600)
                ''', timeout=3, child_args=[str(held)])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'time.sleep(606)', result.stderr)
            self.assert_deletable(held)

    @unittest.skipUnless(sys.platform == 'win32', 'Jobs exist only on Windows')
    def test_kills_hung_grandchild_inside_a_job(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as directory:
            held = Path(directory) / 'held'
            held.write_bytes(b'')
            result, _ = run_wrapper(f'''
                import subprocess, sys
                subprocess.run([sys.executable, '-c', {HOLD_FILE_CODE!r}, sys.argv[1]])
                ''', timeout=3, child_args=[str(held)],
                launcher=[sys.executable, '-c', JOB_LAUNCHER_CODE])
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn(b'Unable to create a job', result.stderr)
            self.assertIn(b'time.sleep(606)', result.stderr)
            self.assert_deletable(held)

    @unittest.skipUnless(sys.platform == 'win32', 'The process tree is walked only on Windows')
    def test_finds_descendants_without_a_job(self):
        import run_with_timeout
        start_time = time.time()
        child = subprocess.Popen(
            [sys.executable, '-c', textwrap.dedent('''
                import subprocess, sys, time
                grandchild = subprocess.Popen(
                    [sys.executable, '-c', 'import time; time.sleep(607)'])
                print(grandchild.pid, flush=True)
                time.sleep(607)
                ''')],
            stdout=subprocess.PIPE)
        try:
            grandchild_pid = int(child.stdout.readline())
            processes = run_with_timeout._windows_process_tree(child.pid, start_time)
            self.assertEqual([p.pid for p in processes], [child.pid, grandchild_pid])
            self.assertEqual([p.name for p in processes], ['python.exe'] * 2)
            self.assertIn('time.sleep(607)', processes[1].command_line)
        finally:
            subprocess.run(['taskkill', '/F', '/T', '/PID', str(child.pid)], capture_output=True)
            child.wait()
            child.stdout.close()

    @unittest.skipUnless(sys.platform == 'win32', 'CPU usage is reported only on Windows')
    def test_reports_cpu_usage_of_hung_processes(self):
        result, _ = run_wrapper('''
            import subprocess, sys
            subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(602)'])
            while True:
                pass
            ''', timeout=3)
        self.assertNotEqual(result.returncode, 0)
        stderr = result.stderr.decode()
        # The printed command lines span several lines, like the child code.
        busy = re.search(
            r'while True.*?CPU: [\d.]+ s, ([\d.]+) s in the last 5 s\s+Busy threads: \d+ \(',
            stderr, re.DOTALL)
        self.assertIsNotNone(busy, stderr)
        self.assertGreater(float(busy.group(1)), 2)
        idle = re.search(r'time.sleep\(602\)"\s+CPU: [\d.]+ s, ([\d.]+) s in the last', stderr)
        self.assertIsNotNone(idle, stderr)
        self.assertLess(float(idle.group(1)), 0.5)
        self.assertRegex(stderr, r'Busiest processes in the last 5 s:\s+[\d.]+ s  \d+ python')
        self.assertRegex(stderr, r'Largest processes by private memory:\s+[\d.]+ GB  \d+ ')
        self.assertRegex(stderr, r'Memory: [\d.]+ of [\d.]+ GB physical memory available')
        self.assertRegex(stderr, r'Disk: [\d.]+ of [\d.]+ GB free')

    @unittest.skipUnless(sys.platform == 'win32', 'Wait chains are reported only on Windows')
    def test_reports_wait_chains(self):
        mutex_name = uuid.uuid4()
        result, _ = run_wrapper(f'''
            import ctypes, subprocess, sys
            kernel32 = ctypes.WinDLL('kernel32')
            kernel32.CreateMutexW.restype = ctypes.c_void_p
            kernel32.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
            holder = subprocess.Popen(
                [sys.executable, '-c',
                    "import ctypes, time; "
                    "ctypes.WinDLL('kernel32').CreateMutexW(None, True, '{mutex_name}'); "
                    "print('locked', flush=True); "
                    "time.sleep(604)"],
                stdout=subprocess.PIPE)
            holder.stdout.readline()
            kernel32.WaitForSingleObject(
                kernel32.CreateMutexW(None, False, '{mutex_name}'), 0xFFFFFFFF)
            ''', timeout=3)
        self.assertNotEqual(result.returncode, 0)
        self.assertRegex(
            result.stderr.decode(),
            r'Thread \d+ waits: thread \d+ of process \d+ \(blocked\) -> mutex .*\(owned\) '
            r'-> thread \d+ of process \d+')

    @unittest.skipUnless(sys.platform == 'win32', 'File holders are reported only on Windows')
    def test_reports_processes_holding_output_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'output.dll'
            output.write_bytes(b'')
            output.with_suffix('.pdb').write_bytes(b'')
            result, _ = run_wrapper('''
                import sys, time
                dll = open(sys.argv[1][len('/out:'):], 'rb')
                time.sleep(605)
                ''', timeout=3, child_args=[f'/out:{output}'])
            self.assertNotEqual(result.returncode, 0)
            stderr = result.stderr.decode()
            self.assertRegex(
                stderr, rf'Processes holding {re.escape(str(output))} open: \d+ [Pp]ython')
            self.assertIn(f'Processes holding {output.with_suffix(".pdb")} open: none', stderr)

    @unittest.skipUnless(sys.platform == 'win32', 'Minidumps are written only on Windows')
    def test_writes_minidumps_of_hung_processes(self):
        with tempfile.TemporaryDirectory() as dump_dir:
            result, _ = run_wrapper('''
                import subprocess, sys
                subprocess.run([sys.executable, '-c', 'import time; time.sleep(603)'])
                ''', timeout=3, wrapper_args=['--dump-dir', dump_dir])
            self.assertNotEqual(result.returncode, 0)
            dumps = sorted(Path(dump_dir).glob('hung_python_*.dmp'))
            self.assertEqual(len(dumps), 2, result.stderr.decode())
            for dump in dumps:
                self.assertIn(f'Minidump: {dump}'.encode(), result.stderr)
                self.assertEqual(dump.read_bytes()[:4], b'MDMP')
                self.assertLess(dump.stat().st_size, 20 * 1024 * 1024)

    def test_finds_output_files_of_link_and_symbol_commands(self):
        import run_with_timeout
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            for name in ('a.dll', 'a.pdb', 'a.lib', 'b.exe', 'b.pdb'):
                (directory / name).write_bytes(b'')
            link = [
                'cmake', '-E', 'vs_link_dll', '--', 'lld-link', '/nologo',
                f'/out:{directory / "a.dll"}', f'/implib:{directory / "a.lib"}',
                f'/PDB:{directory / "a.pdb"}', f'/ilk:{directory / "missing.ilk"}']
            self.assertEqual(
                run_with_timeout._output_files(link),
                [directory / 'a.dll', directory / 'a.lib', directory / 'a.pdb'])
            symbols = [
                'python', 'generate_breakpad_symbols.py', '--binary', str(directory / 'b.exe')]
            self.assertEqual(
                run_with_timeout._output_files(symbols),
                [directory / 'b.exe', directory / 'b.pdb'])

    def test_requires_command(self):
        result = subprocess.run(
            [sys.executable, str(SCRIPT), '--timeout', '1'], capture_output=True, timeout=60)
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
