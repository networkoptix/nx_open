#!/usr/bin/python
'''nx_statistics.storages -- Data model for file storage
'''

from datetime import datetime

import glob
import os

SIZE_TAB = ('', 'KB', 'MB', 'GB', 'TB', 'PB')

def fileSize(size):
    '''File [size] string representation (can be injected)
    '''
    s, i = float(size), 0
    while s >= 1000 and i != len(SIZE_TAB) - 1:
        s /= 1024; i += 1
    return '%0.3f%s' % (s, SIZE_TAB[i])

def fileDate(timestamp):
    '''File [timestamp] string representation (can be inected)
    '''
    dt = datetime.fromtimestamp(timestamp)
    return str(dt)

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
        filePath = self._filePath(**keyArgs)
        self._log.info('Recieved crash report %s, saved to %s (%s)' %
                (keyArgs, filePath, fileSize(len(data))))
        dirPath = os.path.dirname(filePath)
        self._cleanupIfFull(len(bytes(data)))

        if not os.path.exists(dirPath): os.makedirs(dirPath)
        with open(filePath, 'wb') as fd: fd.write(data)
        return self._displayFiles([filePath])[0]

    def read(self, **keyArgs):
        '''Reads file data from file system at [keyArgs]
        '''
        with open(self._filePath(**keyArgs), 'rb') as fd:
            return fd.read()

    def list(self, **keyArgs):
        '''Returns a list of files by [keyArgs] pattern
        '''
        files = glob.glob(self._filePath(**keyArgs))
        return self._displayFiles(files)

    def last(self, count=10):
        '''Returns last [count] files list
        '''
        files = list(f for f in glob.glob(self._filePath())
                       if not os.path.isdir(f))
        files.sort(key=os.path.getctime, reverse=True)
        return self._displayFiles(files[:int(count)])

    def delete(self, **keyArgs):
        '''Deletes crashes by [keyArgs] pattern
        '''
        if not keyArgs: raise KeyError('Forbidden: no params specified')
        filt = self._filePath(**keyArgs)
        files = glob.glob(filt)
        self._log.warning('Removing %i crash reports' % len(files))
        if not files: raise OSError("No such files: %s" % filt)
        for f in files: os.remove(f)

    def _filePath(self, path=None, binary='*', version='*', system='*', timestamp='*', extension='*', **unused):
        '''Calculates absolute file name in file system
        '''
        if path: return os.path.join(self._path, path)
        name = '%s_%s.%s' % (timestamp, system, extension)
        return os.path.join(self._path, binary, version, name)

    def _displayFiles(self, files):
        '''Returns shorted paths, sizes and creation date of [files]
        '''
        def info(path):
            return dict(
                path = path[len(self._path):],
                size = fileSize(os.path.getsize(path)),
                upload = fileDate(os.path.getctime(path)))
        return list(info(f) for f in files if not os.path.isdir(f))

    def _cleanupIfFull(self, neededSize):
        '''Calculates directory size and remove oldest files if [neededSize]
            can not be stored without breaking limit
        '''
        if neededSize > self._limit:
            raise OSError('Report size %s is to big to be saved, limit %s' %
                (fileSize(neededSize), fileSize(self._limit)))
        files = list((f, os.path.getsize(f))
            for f in glob.glob(self._filePath()) if not os.path.isdir(f))
        expected = sum(s for p, s in files) + neededSize
        if expected > self._limit:
            toRemove = []
            files.sort(key=lambda f: os.path.basename(f[0]), reverse=True)
            while expected > self._limit:
                path, size = files.pop()
                expected -= size
                toRemove.append(path)
            self._log.warning('Not enough space, removing: %s' %
                ', '.join(toRemove))
            for path in toRemove: os.remove(path)
        self._log.debug('Storage consumption %s of %s' %
            (fileSize(expected), fileSize(self._limit)))

