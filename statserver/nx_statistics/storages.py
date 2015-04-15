#!/usr/bin/python
'''nx_statistics.storages -- Data model for file storage
'''

import glob
import os

SIZE_TAB = ('', 'KB', 'MB', 'GB', 'TB', 'PB')

def fileSize(size):
    s, i = float(size), 0
    while s >= 1000 and i != len(SIZE_TAB) - 1:
        s /= 1024; i += 1
    return '%0.3f%s' % (s, SIZE_TAB[i])

class FileStorage(object):
    '''File system storage, backed by simple hierarcy:
        [path]/[binary]/[version]/[timestamp]_[system].[extension]
    '''
    def __init__(self, path, limit, log):
        '''Initializes storage to work with [path]
        '''
        self._path = path
        self._limit = limit
        self._log = log

    def write(self, data, **keyArgs):
        '''Writes file data into filesystem according to [keyArgs]
        '''
        # TODO: check up with self._limit before writing, perhaps we should
        #       delete or move oldest files
        filePath = self._filePath(**keyArgs)
        self._log.info('Recieved crash report %s, saved to %s (%s)' %
                (keyArgs, filePath, fileSize(len(data))))
        dirPath = os.path.dirname(filePath)
        if not os.path.exists(dirPath): os.makedirs(dirPath)
        with open(filePath, 'wb') as fd: fd.write(data)

    def read(self, **keyArgs):
        '''Reads file data from file system at [keyArgs]
        '''
        with open(self._filePath(**keyArgs), 'rb') as fd:
            return fd.read()

    def list(self, **keyArgs):
        '''Returns a list of files by [keyArgs] pattern
        '''
        files = glob.glob(self._filePath(**keyArgs))
        return {f[len(self._path):]: fileSize(os.path.getsize(f))
            for f in files if not os.path.isdir(f)}

    def delete(self, **keyArgs):
        '''Deletes crashes by [keyArgs] pattern
        '''
        files = glob.glob(self._filePath(**keyArgs))
        self._log.warning('Removing %i crash reports' % len(files))
        if not files: raise OSError("No such files")
        for f in files: os.remove(f)

    def _filePath(self, path=None, binary='*', version='*', system='*', timestamp='*', extension='*'):
        '''Calculates absolute file name in file system
        '''
        if path: return os.path.join(self._path, path)
        name = '%s_%s.%s' % (timestamp, system, extension)
        return os.path.join(self._path, binary, version, name)

