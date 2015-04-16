#!/usr/bin/python
'''Translates adresses from linux backtraces into function names

Usage:
    cat BACKTRASES | <this-script> > BACKTRASES.resolved

Notes:
    Simbol tables are constructed by nm (see man nm), so all binaries
    (executables and libraries) shell be accessable for this program.
'''

import re
import sys
import subprocess

RE_NM = re.compile(r'([0-9a-f]+)\ ([a-zA-Z]+)\ (.+)')
RE_BT = re.compile(r'(.+)\(\+0x([0-9a-f]+)\)\[0x([0-9a-f]+)\]')

class SimbolTable(object):
    def __init__(self, binary):
        self._table = []
        for line in subprocess.check_output(['nm', binary]).splitlines():
            m = RE_NM.match(line)
            if m:
                addr, t, name = m.groups()
                self._table.append((int(addr, 16), name))
        self._table.sort(key=lambda x: x[0])

    def resolve(self, addr):
        if not self._table: raise KeyError(
            'SimbolTable is empty, cant find 0x%x' & addr)
        begin, end = 0, len(self._table)
        while (end - begin) > 1:
            mid = begin + (end - begin) / 2
            fn, name = self._table[mid]
            if addr < fn: end = mid
            else: begin = mid
        fn, name = self._table[begin]
        if addr < fn: raise KeyError(
            'SimbolTable starts at 0x%x, cand find 0x%x' % (fn, addr))
        return (name, addr - fn)

SIMBOL_TABLES = dict()
def resolveSombol(binary, addr):
    try:
        table = SIMBOL_TABLES[binary]
    except KeyError:
        table = SimbolTable(binary)
        SIMBOL_TABLES[binary] = table
    return table.resolve(addr)

def resolveBacktraseStream(sIn, sOut, sErr=None):
    for line in sIn:
        m = RE_BT.match(line)
        if m:
            binary, addr, ret = m.groups()
            func, addr = resolveSombol(binary, int(addr, 16))
            line = '%s(%s+0x%x)[0x%s]' % (binary, func, addr, ret)
        sOut.write('%s\n' % line)

if __name__ == '__main__':
    resolveBacktraseStream(sys.stdin, sys.stdout)

