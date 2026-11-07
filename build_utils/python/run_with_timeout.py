#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""Runs a command and kills it with all its descendants if it does not finish in time.

A hung build step otherwise blocks the build until the CI job times out, without any hint about
which process got stuck, so the processes are listed before being killed. On Windows, what they
were doing is reported as well: CPU usage, wait chains, minidumps, the processes holding the
output files open and the state of the machine.
"""

import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

# Descendants of a finished command may keep its output pipe open. Waiting for them would hang
# the build the same way as the command itself.
_OUTPUT_GRACE_PERIOD_S = 10
_CPU_SAMPLE_INTERVAL_S = 5
# The diagnostics may get stuck on a machine in a bad state, and must not keep the hung processes
# from being killed.
_DIAGNOSTICS_TIMEOUT_S = 120
# Processes may start while the others are being killed.
_KILL_ROUNDS = 5
_IS_WINDOWS = sys.platform == 'win32'

_OUTPUT_FILE_OPTION = re.compile(r'^[/-](?:out|pdb|implib|ilk):(.+)$', re.IGNORECASE)

if _IS_WINDOWS:
    import windows_process_diagnostics


class _Process(NamedTuple):
    pid: int
    command_line: str
    name: str = ''


class _Report(NamedTuple):
    details: Dict[int, List[str]]
    system: List[str]


def _windows_filetime(unix_time: float) -> int:
    return int((unix_time + 11644473600) * 10**7)


def _windows_descendants(
        root_pid: int, root_start_time: float, snapshot: Dict[int, Tuple[int, str]]) -> Set[int]:
    children = defaultdict(list)
    for pid, (parent_pid, _) in snapshot.items():
        if pid != parent_pid:
            children[parent_pid].append(pid)

    # Windows reuses process ids and keeps the parent id of orphans, so a process only counts as
    # a descendant if it is not older than its parent.
    root_min_created = _windows_filetime(root_start_time - 2)
    result = set()
    root_created = windows_process_diagnostics.creation_time(root_pid)
    if root_created is not None and root_created >= root_min_created:
        result.add(root_pid)
    pending = [(root_pid, root_min_created)]
    while pending:
        pid, min_created = pending.pop()
        for child in children[pid]:
            created = windows_process_diagnostics.creation_time(child)
            if child not in result and created is not None and created >= min_created:
                result.add(child)
                pending.append((child, created))
    return result


def _in_tree_order(root_pid: int, pids: Set[int], parents: Dict[int, int]) -> List[int]:
    children = defaultdict(list)
    for pid in sorted(pids):
        children[parents.get(pid)].append(pid)
    tops = [pid for pid in sorted(pids) if pid != root_pid and parents.get(pid) not in pids]
    if root_pid in pids:
        tops.insert(0, root_pid)
    result = []
    pending = list(reversed(tops))
    while pending:
        pid = pending.pop()
        if pid not in result:
            result.append(pid)
            pending += reversed(children[pid])
    return result + sorted(pids - set(result))


def _windows_process_tree(root_pid: int, root_start_time: float, job=None) -> List[_Process]:
    snapshot = windows_process_diagnostics.process_snapshot()
    if job:
        pids = set(windows_process_diagnostics.job_process_ids(job)) - {os.getpid()}
    else:
        pids = _windows_descendants(root_pid, root_start_time, snapshot)
    result = []
    parents = {pid: parent_pid for pid, (parent_pid, _) in snapshot.items()}
    for pid in _in_tree_order(root_pid, pids, parents):
        name = snapshot.get(pid, (0, ''))[1]
        command_line = windows_process_diagnostics.command_line(pid) or name or '?'
        result.append(_Process(pid, command_line, name))
    return result


def _posix_process_group(group_id: int) -> List[_Process]:
    output = subprocess.run(
        ['ps', '-A', '-o', 'pid=,pgid=,args='],
        stdin=subprocess.DEVNULL, capture_output=True, timeout=120).stdout
    result = []
    for line in output.decode('utf-8', errors='replace').splitlines():
        fields = line.split(None, 2)
        if len(fields) >= 2 and int(fields[1]) == group_id:
            result.append(_Process(int(fields[0]), fields[2] if len(fields) > 2 else ''))
    return result


def _process_tree(root_pid: int, root_start_time: float, job) -> List[_Process]:
    try:
        if _IS_WINDOWS:
            return _windows_process_tree(root_pid, root_start_time, job)
        # The command runs in its own session, so orphaned descendants are still found.
        return _posix_process_group(root_pid)
    except Exception as e:
        print(f'WARNING: Unable to list processes: {e!r}', file=sys.stderr)
        return []


def _output_files(command: List[str]) -> List[Path]:
    """Finds the files written by a link or symbol generation command."""
    files = []
    for i, arg in enumerate(command):
        if match := _OUTPUT_FILE_OPTION.match(arg):
            files.append(Path(match.group(1).strip('"')))
        elif arg == '--binary' and i + 1 < len(command):
            files.append(Path(command[i + 1]))
    files += [f.with_suffix('.pdb') for f in files if f.suffix.lower() in ('.dll', '.exe')]
    return [f for f in dict.fromkeys(files) if f.is_file()]


def _report_step(description: str, step):
    try:
        step()
    except Exception as e:
        print(f'WARNING: Unable to {description}: {e!r}', file=sys.stderr, flush=True)


def _top_processes(values: Dict[int, float], names: Dict[int, str], format_value, minimum, count):
    top = sorted(((v, pid) for pid, v in values.items() if v >= minimum), reverse=True)[:count]
    return [f'        {format_value(v)}  {pid} {names.get(pid, "?")}' for v, pid in top]


def _report_cpu_usage(processes: List[_Process], before, after, report: _Report):
    for process in processes:
        pid = process.pid
        if pid not in after.cpu_s:
            continue
        cpu = f'CPU: {after.cpu_s[pid]:.1f} s'
        if pid in before.cpu_s:
            cpu += (
                f', {after.cpu_s[pid] - before.cpu_s[pid]:.2f} s '
                f'in the last {_CPU_SAMPLE_INTERVAL_S} s')
        report.details[pid].append(cpu)
        thread_cpu_before = before.thread_cpu_s.get(pid, {})
        busy_threads = sorted(
            ((s - thread_cpu_before.get(tid, 0), tid)
                for tid, s in after.thread_cpu_s.get(pid, {}).items()),
            reverse=True)
        busy_threads = [f'{tid} ({s:.2f} s)' for s, tid in busy_threads if s >= 0.1]
        if busy_threads:
            report.details[pid].append(f'Busy threads: {", ".join(busy_threads)}')

    cpu_delta = {pid: s - before.cpu_s.get(pid, 0) for pid, s in after.cpu_s.items()}
    report.system.append(f'Busiest processes in the last {_CPU_SAMPLE_INTERVAL_S} s:')
    report.system.extend(_top_processes(
        cpu_delta, after.names, lambda s: f'{s:6.2f} s', minimum=0.1, count=10))
    report.system.append('Largest processes by private memory:')
    report.system.extend(_top_processes(
        after.private_bytes, after.names, lambda b: f'{b / 1024**3:6.2f} GB',
        minimum=0, count=5))


def _report_minidumps(processes: List[_Process], dump_dir: Path, report: _Report):
    dump_dir.mkdir(parents=True, exist_ok=True)
    for process in processes:
        path = windows_process_diagnostics.write_minidump(process.pid, process.name, dump_dir)
        report.details[process.pid].append(f'Minidump: {path}')


def _report_wait_chains(processes: List[_Process], sample, report: _Report):
    for process in processes:
        thread_ids = sample.thread_cpu_s.get(process.pid, {}).keys()
        for tid, chain in windows_process_diagnostics.wait_chains(thread_ids).items():
            report.details[process.pid].append(f'Thread {tid} waits: {chain}')


def _report_file_holders(command: List[str], report: _Report):
    for path in _output_files(command):
        holders = windows_process_diagnostics.file_holders(path)
        holder_list = ', '.join(f'{pid} {name}' for pid, name in holders) or 'none'
        report.system.append(f'Processes holding {path} open: {holder_list}')


def _report_machine_state(report: _Report):
    report.system.append(f'Memory: {windows_process_diagnostics.memory_status()}')
    usage = shutil.disk_usage('.')
    report.system.append(
        f'Disk: {usage.free / 1024**3:.1f} of {usage.total / 1024**3:.1f} GB free '
        f'in {os.getcwd()}')


def _windows_diagnose(
        processes: List[_Process], command: List[str], dump_dir: Optional[Path],
        report: _Report):
    # The minidumps go first, so they still catch a process that ends during the diagnostics.
    if dump_dir:
        _report_step(
            'write minidumps', lambda: _report_minidumps(processes, dump_dir, report))
    pids = {p.pid for p in processes}
    try:
        before = windows_process_diagnostics.take_sample(pids)
        _report_step(
            'follow wait chains', lambda: _report_wait_chains(processes, before, report))
        time.sleep(_CPU_SAMPLE_INTERVAL_S)
        after = windows_process_diagnostics.take_sample(pids)
        _report_step(
            'report CPU usage', lambda: _report_cpu_usage(processes, before, after, report))
    except Exception as e:
        print(f'WARNING: Unable to measure CPU usage: {e!r}', file=sys.stderr, flush=True)
    _report_step(
        'find the processes holding the output files',
        lambda: _report_file_holders(command, report))
    _report_step('read the machine state', lambda: _report_machine_state(report))


def _diagnose(
        processes: List[_Process], command: List[str], dump_dir: Optional[Path]) -> _Report:
    report = _Report(defaultdict(list), [])
    if not _IS_WINDOWS:
        return report
    diagnostics = threading.Thread(
        target=_windows_diagnose, args=(processes, command, dump_dir, report), daemon=True)
    diagnostics.start()
    diagnostics.join(_DIAGNOSTICS_TIMEOUT_S)
    if diagnostics.is_alive():
        print(
            f'WARNING: Diagnostics did not finish in {_DIAGNOSTICS_TIMEOUT_S} seconds.',
            file=sys.stderr, flush=True)
    return report


def _kill(root_pid: int, processes: List[_Process], job):
    if not _IS_WINDOWS:
        try:
            os.killpg(root_pid, signal.SIGKILL)
        except OSError:
            pass
        return
    pids = {p.pid for p in processes} or {root_pid}
    for _ in range(_KILL_ROUNDS):
        for pid in pids:
            try:
                os.kill(pid, signal.SIGTERM)  #< TerminateProcess() on Windows.
            except OSError:
                pass
        if not job:
            return
        time.sleep(0.1)
        try:
            pids = set(windows_process_diagnostics.job_process_ids(job)) - {os.getpid()}
        except Exception as e:
            print(f'WARNING: Unable to list the processes of the job: {e!r}', file=sys.stderr)
            return
        if not pids:
            return


def _print_processes(message: str, processes: List[_Process], report: Optional[_Report] = None):
    lines = [message]
    for p in processes:
        lines.append(f'    {p.pid}: {p.command_line}')
        if report:
            lines += [f'        {line}' for line in list(report.details.get(p.pid, []))]
    if not processes:
        lines.append('    (none found)')
    if report and report.system:
        lines.append('State of the machine:')
        lines += [f'    {line}' for line in list(report.system)]
    print('\n'.join(lines), file=sys.stderr, flush=True)


def _read_output(stream, chunks: List[bytes]):
    while chunk := stream.read1(65536):
        chunks.append(chunk)


def _write_output(chunks: List[bytes]):
    sys.stdout.buffer.write(b''.join(list(chunks)))
    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--timeout', type=float, required=True, help='Seconds to wait for the command.')
    parser.add_argument(
        '--dump-dir', type=Path,
        help='Directory for the minidumps of the processes of a hung command (Windows only).')
    parser.add_argument('command', nargs='+', help='Command to run, preceded by "--".')
    args = parser.parse_args()
    command_line = subprocess.list2cmdline(args.command)

    job = None
    if _IS_WINDOWS:
        try:
            job = windows_process_diagnostics.create_job()
        except Exception as e:
            print(
                f'WARNING: Unable to create a job for the processes: {e!r}',
                file=sys.stderr, flush=True)

    start_time = time.time()
    process = subprocess.Popen(
        args.command,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        start_new_session=not _IS_WINDOWS)
    chunks = []
    reader = threading.Thread(target=_read_output, args=(process.stdout, chunks), daemon=True)
    reader.start()

    try:
        process.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        processes = _process_tree(process.pid, start_time, job)
        report = _diagnose(processes, args.command, args.dump_dir)
        _kill(process.pid, processes, job)
        reader.join(_OUTPUT_GRACE_PERIOD_S)
        _write_output(chunks)
        _print_processes(
            f'ERROR: {command_line} did not finish in {args.timeout:g} seconds. '
            'Killed these processes:',
            processes,
            report)
        return 1
    except KeyboardInterrupt:
        _kill(process.pid, _process_tree(process.pid, start_time, job), job)
        raise

    reader.join(_OUTPUT_GRACE_PERIOD_S)
    if reader.is_alive():
        processes = _process_tree(process.pid, start_time, job)
        _kill(process.pid, processes, job)
        _write_output(chunks)
        _print_processes(
            f'WARNING: {command_line} exited with code {process.returncode}, but its output is '
            'still held open by these processes, which are killed:',
            processes)
        return process.returncode
    _write_output(chunks)
    return process.returncode


if __name__ == '__main__':
    sys.exit(main())
