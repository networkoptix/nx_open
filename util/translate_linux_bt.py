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
        self._binary = binary
        self._table = self._readBinary(binary)
        if not self._table:
            self._table = self._readBinary(binary, '--dynamic')
        self._table.sort(key=lambda x: x[0])

    def resolve(self, addr):
        begin, end = 0, len(self._table)
        while (end - begin) > 1:
            mid = begin + (end - begin) / 2
            fn, name = self._table[mid]
            if addr < fn: end = mid
            else: begin = mid
        fn, name = self._table[begin]
        if addr < fn:
            m = 'SimbolTable %s starts at 0x%x, cand find 0x%x'
            raise KeyError(m % (self._binary, fn, addr))
        return (name, addr - fn)

    def __nonzero__(self):
        return bool(self._table)

    @staticmethod
    def _readBinary(binary, *options):
        table = []
        nm = subprocess.check_output(['nm', binary] + list(options))
        for line in nm.splitlines():
            m = RE_NM.match(line)
            if m:
                addr, t, name = m.groups()
                table.append((int(addr, 16), name))
        return table

SIMBOL_TABLES = dict()
def resolveSombol(binary, addr):
    try:
        table = SIMBOL_TABLES[binary]
    except KeyError:
        table = SimbolTable(binary)
        SIMBOL_TABLES[binary] = table
    if not table: return None
    return '%s+0x%x' % table.resolve(addr)

def resolveBacktraseStream(sIn, sOut):
    for line in sIn:
        m = RE_BT.match(line)
        if m:
            binary, addr, ret = m.groups()
            func = resolveSombol(binary, int(addr, 16))
            if func: line = '%s(%s)[0x%s]\n' % (binary, func, ret)
        sOut.write(line)

if __name__ == '__main__':
    resolveBacktraseStream(sys.stdin, sys.stdout)

