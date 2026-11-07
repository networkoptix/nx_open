## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""Tells what hung Windows processes are doing, using only the Windows API.

The build machines have no debugging tools installed, so everything goes through ctypes.
"""

import ctypes
import msvcrt
import re
from ctypes import wintypes
from pathlib import Path
from typing import Dict, Iterable, List, NamedTuple, Optional, Set, Tuple

_PROCESS_VM_READ = 0x0010
_PROCESS_DUP_HANDLE = 0x0040
_PROCESS_QUERY_INFORMATION = 0x0400
_PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
_THREAD_QUERY_LIMITED_INFORMATION = 0x0800
_TH32CS_SNAPPROCESS = 0x2
_TH32CS_SNAPTHREAD = 0x4
_INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
_ERROR_MORE_DATA = 234
_JOB_OBJECT_LIMIT_BREAKAWAY_OK = 0x0800
_JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x2000
_JOB_OBJECT_BASIC_PROCESS_ID_LIST = 3
_JOB_OBJECT_EXTENDED_LIMIT_INFORMATION = 9
_PROCESS_COMMAND_LINE_INFORMATION = 60
_STATUS_INFO_LENGTH_MISMATCH = 0xC0000004
_FILE_READ_ATTRIBUTES = 0x0080
_FILE_SHARE_READ_WRITE_DELETE = 0x7
_OPEN_EXISTING = 3
_FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
_FILE_PROCESS_IDS_USING_FILE_INFORMATION = 47

# Thread stacks, thread times and handles, but no heap contents: a dump of a linker with its whole
# memory could take gigabytes.
_MINIDUMP_TYPE = (
    0x0004  #< MiniDumpWithHandleData
    | 0x0020  #< MiniDumpWithUnloadedModules
    | 0x1000)  #< MiniDumpWithThreadInfo

# Out-of-process waits, critical sections and network I/O. Following COM calls would need a
# registered callback.
_WCT_FLAGS = 0x1 | 0x4 | 0x8
_WCT_MAX_NODE_COUNT = 16
_WCT_THREAD_TYPE = 8
_WCT_OBJECT_TYPES = {
    1: 'critical section',
    2: 'SendMessage',
    3: 'mutex',
    4: 'ALPC',
    5: 'COM',
    6: 'thread wait',
    7: 'process wait',
    8: 'thread',
    9: 'COM activation',
    10: 'unknown',
    11: 'socket I/O',
    12: 'SMB I/O',
}
_WCT_STATUSES = {
    1: 'no access',
    2: 'running',
    3: 'blocked',
    4: 'pid only',
    5: 'pid only, RPCSS',
    6: 'owned',
    7: 'not owned',
    8: 'abandoned',
    9: 'unknown',
    10: 'error',
}


class _PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('cntUsage', wintypes.DWORD),
        ('th32ProcessID', wintypes.DWORD),
        ('th32DefaultHeapID', ctypes.c_size_t),
        ('th32ModuleID', wintypes.DWORD),
        ('cntThreads', wintypes.DWORD),
        ('th32ParentProcessID', wintypes.DWORD),
        ('pcPriClassBase', wintypes.LONG),
        ('dwFlags', wintypes.DWORD),
        ('szExeFile', wintypes.WCHAR * 260)]


class _THREADENTRY32(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('cntUsage', wintypes.DWORD),
        ('th32ThreadID', wintypes.DWORD),
        ('th32OwnerProcessID', wintypes.DWORD),
        ('tpBasePri', wintypes.LONG),
        ('tpDeltaPri', wintypes.LONG),
        ('dwFlags', wintypes.DWORD)]


class _PROCESS_MEMORY_COUNTERS_EX(ctypes.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            'PeakWorkingSetSize',
            'WorkingSetSize',
            'QuotaPeakPagedPoolUsage',
            'QuotaPagedPoolUsage',
            'QuotaPeakNonPagedPoolUsage',
            'QuotaNonPagedPoolUsage',
            'PagefileUsage',
            'PeakPagefileUsage',
            'PrivateUsage')]


class _MEMORYSTATUSEX(ctypes.Structure):
    _fields_ = [('dwLength', wintypes.DWORD), ('dwMemoryLoad', wintypes.DWORD)] + [
        (name, ctypes.c_uint64) for name in (
            'ullTotalPhys',
            'ullAvailPhys',
            'ullTotalPageFile',
            'ullAvailPageFile',
            'ullTotalVirtual',
            'ullAvailVirtual',
            'ullAvailExtendedVirtual')]


class _IO_COUNTERS(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_uint64) for name in (
            'ReadOperationCount',
            'WriteOperationCount',
            'OtherOperationCount',
            'ReadTransferCount',
            'WriteTransferCount',
            'OtherTransferCount')]


class _JOBOBJECT_BASIC_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [
        ('PerProcessUserTimeLimit', ctypes.c_int64),
        ('PerJobUserTimeLimit', ctypes.c_int64),
        ('LimitFlags', wintypes.DWORD),
        ('MinimumWorkingSetSize', ctypes.c_size_t),
        ('MaximumWorkingSetSize', ctypes.c_size_t),
        ('ActiveProcessLimit', wintypes.DWORD),
        ('Affinity', ctypes.c_size_t),
        ('PriorityClass', wintypes.DWORD),
        ('SchedulingClass', wintypes.DWORD)]


class _JOBOBJECT_EXTENDED_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [
        ('BasicLimitInformation', _JOBOBJECT_BASIC_LIMIT_INFORMATION),
        ('IoInfo', _IO_COUNTERS),
        ('ProcessMemoryLimit', ctypes.c_size_t),
        ('JobMemoryLimit', ctypes.c_size_t),
        ('PeakProcessMemoryUsed', ctypes.c_size_t),
        ('PeakJobMemoryUsed', ctypes.c_size_t)]


def _job_process_id_list_type(count: int):
    class _JOBOBJECT_BASIC_PROCESS_ID_LIST(ctypes.Structure):
        _fields_ = [
            ('NumberOfAssignedProcesses', wintypes.DWORD),
            ('NumberOfProcessIdsInList', wintypes.DWORD),
            ('ProcessIdList', ctypes.c_size_t * count)]
    return _JOBOBJECT_BASIC_PROCESS_ID_LIST


def _file_process_id_list_type(count: int):
    class _FILE_PROCESS_IDS_USING_FILE_INFORMATION(ctypes.Structure):
        _fields_ = [
            ('NumberOfProcessIdsInList', wintypes.ULONG),
            ('ProcessIdList', ctypes.c_size_t * count)]
    return _FILE_PROCESS_IDS_USING_FILE_INFORMATION


class _IO_STATUS_BLOCK(ctypes.Structure):
    _fields_ = [('Status', ctypes.c_void_p), ('Information', ctypes.c_size_t)]


class _UNICODE_STRING(ctypes.Structure):
    _fields_ = [
        ('Length', wintypes.USHORT),
        ('MaximumLength', wintypes.USHORT),
        ('Buffer', ctypes.c_void_p)]


class _WCT_LOCK_OBJECT(ctypes.Structure):
    _fields_ = [
        ('ObjectName', wintypes.WCHAR * 128),
        ('Timeout', ctypes.c_int64),
        ('Alertable', wintypes.BOOL)]


class _WCT_THREAD_OBJECT(ctypes.Structure):
    _fields_ = [
        ('ProcessId', wintypes.DWORD),
        ('ThreadId', wintypes.DWORD),
        ('WaitTime', wintypes.DWORD),
        ('ContextSwitches', wintypes.DWORD)]


class _WCT_OBJECT(ctypes.Union):
    _fields_ = [('LockObject', _WCT_LOCK_OBJECT), ('ThreadObject', _WCT_THREAD_OBJECT)]


class _WAITCHAIN_NODE_INFO(ctypes.Structure):
    _anonymous_ = ('object',)
    _fields_ = [
        ('ObjectType', ctypes.c_int),
        ('ObjectStatus', ctypes.c_int),
        ('object', _WCT_OBJECT)]


def _declare(library, name: str, restype, *argtypes):
    function = getattr(library, name)
    function.restype = restype
    function.argtypes = argtypes
    return function


_kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
_OpenProcess = _declare(
    _kernel32, 'OpenProcess', wintypes.HANDLE, wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
_OpenThread = _declare(
    _kernel32, 'OpenThread', wintypes.HANDLE, wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
_CloseHandle = _declare(_kernel32, 'CloseHandle', wintypes.BOOL, wintypes.HANDLE)
_CreateFileW = _declare(
    _kernel32, 'CreateFileW', wintypes.HANDLE,
    wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
    wintypes.DWORD, wintypes.HANDLE)
_GetCurrentProcess = _declare(_kernel32, 'GetCurrentProcess', wintypes.HANDLE)
_CreateJobObjectW = _declare(
    _kernel32, 'CreateJobObjectW', wintypes.HANDLE, ctypes.c_void_p, wintypes.LPCWSTR)
_SetInformationJobObject = _declare(
    _kernel32, 'SetInformationJobObject', wintypes.BOOL,
    wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD)
_QueryInformationJobObject = _declare(
    _kernel32, 'QueryInformationJobObject', wintypes.BOOL,
    wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD))
_AssignProcessToJobObject = _declare(
    _kernel32, 'AssignProcessToJobObject', wintypes.BOOL, wintypes.HANDLE, wintypes.HANDLE)
_FILETIME_POINTERS = [ctypes.POINTER(ctypes.c_uint64)] * 4
_GetProcessTimes = _declare(
    _kernel32, 'GetProcessTimes', wintypes.BOOL, wintypes.HANDLE, *_FILETIME_POINTERS)
_GetThreadTimes = _declare(
    _kernel32, 'GetThreadTimes', wintypes.BOOL, wintypes.HANDLE, *_FILETIME_POINTERS)
_GetProcessMemoryInfo = _declare(
    _kernel32, 'K32GetProcessMemoryInfo', wintypes.BOOL,
    wintypes.HANDLE, ctypes.POINTER(_PROCESS_MEMORY_COUNTERS_EX), wintypes.DWORD)
_GlobalMemoryStatusEx = _declare(
    _kernel32, 'GlobalMemoryStatusEx', wintypes.BOOL, ctypes.POINTER(_MEMORYSTATUSEX))
_CreateToolhelp32Snapshot = _declare(
    _kernel32, 'CreateToolhelp32Snapshot', wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD)
_Process32FirstW = _declare(
    _kernel32, 'Process32FirstW', wintypes.BOOL,
    wintypes.HANDLE, ctypes.POINTER(_PROCESSENTRY32W))
_Process32NextW = _declare(
    _kernel32, 'Process32NextW', wintypes.BOOL,
    wintypes.HANDLE, ctypes.POINTER(_PROCESSENTRY32W))
_Thread32First = _declare(
    _kernel32, 'Thread32First', wintypes.BOOL, wintypes.HANDLE, ctypes.POINTER(_THREADENTRY32))
_Thread32Next = _declare(
    _kernel32, 'Thread32Next', wintypes.BOOL, wintypes.HANDLE, ctypes.POINTER(_THREADENTRY32))

_ntdll = ctypes.WinDLL('ntdll')
_NtQueryInformationProcess = _declare(
    _ntdll, 'NtQueryInformationProcess', wintypes.LONG,
    wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.ULONG,
    ctypes.POINTER(wintypes.ULONG))
_NtQueryInformationFile = _declare(
    _ntdll, 'NtQueryInformationFile', wintypes.LONG,
    wintypes.HANDLE, ctypes.POINTER(_IO_STATUS_BLOCK), ctypes.c_void_p, wintypes.ULONG,
    ctypes.c_int)

_dbghelp = ctypes.WinDLL('dbghelp', use_last_error=True)
_MiniDumpWriteDump = _declare(
    _dbghelp, 'MiniDumpWriteDump', wintypes.BOOL,
    wintypes.HANDLE, wintypes.DWORD, wintypes.HANDLE, wintypes.DWORD,
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)

_advapi32 = ctypes.WinDLL('advapi32', use_last_error=True)
_OpenThreadWaitChainSession = _declare(
    _advapi32, 'OpenThreadWaitChainSession', wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p)
_CloseThreadWaitChainSession = _declare(
    _advapi32, 'CloseThreadWaitChainSession', None, wintypes.HANDLE)
_GetThreadWaitChain = _declare(
    _advapi32, 'GetThreadWaitChain', wintypes.BOOL,
    wintypes.HANDLE, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(_WAITCHAIN_NODE_INFO),
    ctypes.POINTER(wintypes.BOOL))

class Sample(NamedTuple):
    names: Dict[int, str]
    cpu_s: Dict[int, float]
    private_bytes: Dict[int, int]
    #< Only for the watched processes.
    thread_cpu_s: Dict[int, Dict[int, float]]


def _walk_snapshot(snapshot, entry_type, first, next_):
    entry = entry_type(dwSize=ctypes.sizeof(entry_type))
    has_entry = first(snapshot, ctypes.byref(entry))
    while has_entry:
        yield entry
        has_entry = next_(snapshot, ctypes.byref(entry))


def _create_snapshot(flags: int):
    snapshot = _CreateToolhelp32Snapshot(flags, 0)
    if snapshot == _INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    return snapshot


def _snapshot_processes(snapshot) -> Dict[int, Tuple[int, str]]:
    return {
        entry.th32ProcessID: (entry.th32ParentProcessID, entry.szExeFile)
        for entry in _walk_snapshot(
            snapshot, _PROCESSENTRY32W, _Process32FirstW, _Process32NextW)}


def process_snapshot() -> Dict[int, Tuple[int, str]]:
    """Maps the ids of all processes to their parent ids and executable names."""
    snapshot = _create_snapshot(_TH32CS_SNAPPROCESS)
    try:
        return _snapshot_processes(snapshot)
    finally:
        _CloseHandle(snapshot)


def _times(handle, get_times) -> Optional[List[int]]:
    times = [ctypes.c_uint64() for _ in range(4)]  #< Creation, exit, kernel, user.
    if not get_times(handle, *[ctypes.byref(t) for t in times]):
        return None
    return [t.value for t in times]


def _cpu_seconds(handle, get_times):
    times = _times(handle, get_times)
    return None if times is None else (times[2] + times[3]) / 10**7


def creation_time(pid: int) -> Optional[int]:
    """Returns the creation time of the process as a FILETIME value."""
    handle = _OpenProcess(_PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return None
    try:
        times = _times(handle, _GetProcessTimes)
    finally:
        _CloseHandle(handle)
    return None if times is None else times[0]


def command_line(pid: int) -> Optional[str]:
    handle = _OpenProcess(_PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return None
    try:
        size = wintypes.ULONG(1024)
        for _ in range(4):
            buffer = ctypes.create_string_buffer(size.value)
            status = _NtQueryInformationProcess(
                handle, _PROCESS_COMMAND_LINE_INFORMATION, buffer, size, ctypes.byref(size))
            if status & 0xFFFFFFFF != _STATUS_INFO_LENGTH_MISMATCH:
                break
        if status != 0:
            return None
        string = _UNICODE_STRING.from_buffer(buffer)
        return ctypes.wstring_at(string.Buffer, string.Length // 2) if string.Buffer else ''
    finally:
        _CloseHandle(handle)


def create_job():
    """Puts the current process into a new job, and returns the job handle.

    The processes started afterwards inherit the job, even if their parent exits, so the job
    lists all of them. Closing the job handle, also by exiting, kills the ones still running.
    """
    job = _CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())
    limits = _JOBOBJECT_EXTENDED_LIMIT_INFORMATION()
    # Without BREAKAWAY_OK, a process asking to leave the job would fail to start.
    limits.BasicLimitInformation.LimitFlags = (
        _JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | _JOB_OBJECT_LIMIT_BREAKAWAY_OK)
    if not (
            _SetInformationJobObject(
                job, _JOB_OBJECT_EXTENDED_LIMIT_INFORMATION,
                ctypes.byref(limits), ctypes.sizeof(limits))
            and _AssignProcessToJobObject(job, _GetCurrentProcess())):
        error = ctypes.get_last_error()
        _CloseHandle(job)
        raise ctypes.WinError(error)
    return job


def job_process_ids(job) -> List[int]:
    count = 64
    while True:
        id_list = _job_process_id_list_type(count)()
        if not _QueryInformationJobObject(
                job, _JOB_OBJECT_BASIC_PROCESS_ID_LIST,
                ctypes.byref(id_list), ctypes.sizeof(id_list), None):
            error = ctypes.get_last_error()
            if error != _ERROR_MORE_DATA:
                raise ctypes.WinError(error)
        if id_list.NumberOfAssignedProcesses <= id_list.NumberOfProcessIdsInList:
            return list(id_list.ProcessIdList[:id_list.NumberOfProcessIdsInList])
        count = id_list.NumberOfAssignedProcesses + 16


def take_sample(watched_pids: Set[int]) -> Sample:
    """Reads the CPU times and memory usage of all processes and the threads of watched ones."""
    snapshot = _create_snapshot(_TH32CS_SNAPPROCESS | _TH32CS_SNAPTHREAD)
    try:
        names = {pid: name for pid, (_, name) in _snapshot_processes(snapshot).items()}
        thread_ids = {pid: [] for pid in watched_pids}
        for entry in _walk_snapshot(snapshot, _THREADENTRY32, _Thread32First, _Thread32Next):
            if entry.th32OwnerProcessID in thread_ids:
                thread_ids[entry.th32OwnerProcessID].append(entry.th32ThreadID)
    finally:
        _CloseHandle(snapshot)

    cpu_s = {}
    private_bytes = {}
    for pid in names:
        handle = _OpenProcess(_PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
        if not handle:
            continue
        try:
            seconds = _cpu_seconds(handle, _GetProcessTimes)
            if seconds is not None:
                cpu_s[pid] = seconds
            counters = _PROCESS_MEMORY_COUNTERS_EX()
            if _GetProcessMemoryInfo(handle, ctypes.byref(counters), ctypes.sizeof(counters)):
                private_bytes[pid] = counters.PrivateUsage
        finally:
            _CloseHandle(handle)

    thread_cpu_s = {}
    for pid, tids in thread_ids.items():
        thread_cpu_s[pid] = {}
        for tid in tids:
            handle = _OpenThread(_THREAD_QUERY_LIMITED_INFORMATION, False, tid)
            if not handle:
                continue
            try:
                seconds = _cpu_seconds(handle, _GetThreadTimes)
            finally:
                _CloseHandle(handle)
            if seconds is not None:
                thread_cpu_s[pid][tid] = seconds
    return Sample(names, cpu_s, private_bytes, thread_cpu_s)


def _describe_wait_chain_node(node: _WAITCHAIN_NODE_INFO) -> str:
    status = _WCT_STATUSES.get(node.ObjectStatus, f'status {node.ObjectStatus}')
    if node.ObjectType == _WCT_THREAD_TYPE:
        thread = node.ThreadObject
        return f'thread {thread.ThreadId} of process {thread.ProcessId} ({status})'
    kind = _WCT_OBJECT_TYPES.get(node.ObjectType, f'object of type {node.ObjectType}')
    name = node.LockObject.ObjectName
    return f'{kind} {name!r} ({status})' if name else f'{kind} ({status})'


def wait_chains(thread_ids: Iterable[int]) -> Dict[int, str]:
    """Describes what the threads wait for, if Wait Chain Traversal can follow it."""
    session = _OpenThreadWaitChainSession(0, None)
    if not session:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        result = {}
        for tid in thread_ids:
            nodes = (_WAITCHAIN_NODE_INFO * _WCT_MAX_NODE_COUNT)()
            count = wintypes.DWORD(_WCT_MAX_NODE_COUNT)
            is_cycle = wintypes.BOOL()
            if not _GetThreadWaitChain(
                    session, 0, _WCT_FLAGS, tid,
                    ctypes.byref(count), nodes, ctypes.byref(is_cycle)):
                continue
            # A single node is the thread itself, waiting for something WCT does not follow.
            if count.value < 2 and not is_cycle:
                continue
            chain = ' -> '.join(_describe_wait_chain_node(n) for n in nodes[:count.value])
            result[tid] = f'DEADLOCK: {chain}' if is_cycle else chain
        return result
    finally:
        _CloseThreadWaitChainSession(session)


def file_holders(path: Path) -> List[Tuple[int, str]]:
    """Lists the processes having the file open."""
    handle = _CreateFileW(
        str(path), _FILE_READ_ATTRIBUTES, _FILE_SHARE_READ_WRITE_DELETE, None, _OPEN_EXISTING,
        _FILE_FLAG_BACKUP_SEMANTICS, None)
    if handle == _INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        count = 64
        while True:
            id_list = _file_process_id_list_type(count)()
            status = _NtQueryInformationFile(
                handle, ctypes.byref(_IO_STATUS_BLOCK()), ctypes.byref(id_list),
                ctypes.sizeof(id_list), _FILE_PROCESS_IDS_USING_FILE_INFORMATION)
            if status & 0xFFFFFFFF != _STATUS_INFO_LENGTH_MISMATCH:
                break
            count *= 4
        if status != 0:
            raise OSError(
                f'NtQueryInformationFile() failed with status {status & 0xFFFFFFFF:#010x}')
        pids = id_list.ProcessIdList[:id_list.NumberOfProcessIdsInList]
    finally:
        _CloseHandle(handle)
    snapshot = process_snapshot()
    return [(pid, snapshot.get(pid, (0, '?'))[1]) for pid in pids]


def memory_status() -> str:
    status = _MEMORYSTATUSEX(dwLength=ctypes.sizeof(_MEMORYSTATUSEX))
    if not _GlobalMemoryStatusEx(ctypes.byref(status)):
        raise ctypes.WinError(ctypes.get_last_error())
    gb = 1024**3
    committed = status.ullTotalPageFile - status.ullAvailPageFile
    return (
        f'{status.ullAvailPhys / gb:.1f} of {status.ullTotalPhys / gb:.1f} GB physical memory '
        f'available, {committed / gb:.1f} of {status.ullTotalPageFile / gb:.1f} GB committed')


def write_minidump(pid: int, name: str, dump_dir: Path) -> str:
    """Returns the path of the minidump, or the reason it could not be written."""
    safe_name = re.sub(r'[^\w.-]', '_', Path(name).stem) or 'process'
    path = dump_dir / f'hung_{safe_name}_{pid}.dmp'
    handle = _OpenProcess(
        _PROCESS_QUERY_INFORMATION | _PROCESS_VM_READ | _PROCESS_DUP_HANDLE, False, pid)
    if not handle:
        return f'unable to open the process (error {ctypes.get_last_error()})'
    try:
        with open(path, 'wb') as f:
            written = _MiniDumpWriteDump(
                handle, pid, msvcrt.get_osfhandle(f.fileno()), _MINIDUMP_TYPE, None, None, None)
            error = ctypes.get_last_error()
        if not written:
            path.unlink()
            return f'unable to write a minidump (error {error & 0xFFFFFFFF:#010x})'
    finally:
        _CloseHandle(handle)
    return str(path)
