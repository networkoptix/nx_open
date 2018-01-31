"""Load memory usage metrics for a hostname
"""

from collections import namedtuple


LIGHTWEIGHT_SERVER_BINARY_NAME = 'appserver2_ut'
MEDIASERVER_BINARY_NAME = 'mediaserver-bin'


MemoryUsage = namedtuple('MemoryUsage', 'total used free used_swap mediaserver lws')

_FreeMemory = namedtuple('_FreeMemory', 'total used free used_swap')
_PsMemory = namedtuple('_PsMemory', 'mediaserver lws')


# We accept following output variants from 'free' command:
#
#              total       used       free     shared    buffers     cached
# Mem:      65867796   19843624   46024172        116     327184    7782192
# -/+ buffers/cache:   11734248   54133548
# Swap:     33442812      76116   33366696
#
#               total        used        free      shared  buff/cache   available
# Mem:       32850392    12854660      247240      477276    19748492    19052900
# Swap:      33452028      586264    32865764

def _load_host_free_memory(os_access):
    lines = os_access.run_command(['free', '--bytes']).splitlines()
    key2line = dict([line.split(':') for line in lines[1:]])  # skip first line with titles
    mem_counts = key2line['Mem'].split()
    buf_cache_line = key2line.get('-/+ buffers/cache')
    total = mem_counts[0]
    if buf_cache_line:
        buf_cache_counts = buf_cache_line.split()
        used = buf_cache_counts[0]
        free = buf_cache_counts[1]
    else:
        used = mem_counts[1]
        free = mem_counts[2]
    used_swap = key2line['Swap'].split()[1]
    return _FreeMemory(
        total=int(total),
        used=int(used),
        free=int(free),
        used_swap=int(used_swap),
        )

def _load_server_memory_usage(os_access):
    lines = os_access.run_command(['ps', 'xl']).splitlines()
    idx2column = dict(enumerate(lines[0].split()))
    mediaserver_usage = 0
    lws_usage = 0
    for line in lines[1:]:
        column2value = {idx2column[idx].lower(): value for idx, value in enumerate(line.split(None, len(idx2column) - 1))}
        cmdline = column2value['command']
        rss = int(column2value['rss']) * 1024  # ps output is in kilobytes
        if MEDIASERVER_BINARY_NAME in cmdline:
            mediaserver_usage += rss
        if LIGHTWEIGHT_SERVER_BINARY_NAME in cmdline:
            lws_usage += rss
    return _PsMemory(
        mediaserver=mediaserver_usage,
        lws=lws_usage,
        )

def load_host_memory_usage(os_access):
    free_memory = _load_host_free_memory(os_access)
    ps_memory = _load_server_memory_usage(os_access)
    return MemoryUsage(
        total=free_memory.total,
        used=free_memory.used,
        used_swap=free_memory.used_swap,
        free=free_memory.free,
        mediaserver=ps_memory.mediaserver,
        lws=ps_memory.lws,
        )
