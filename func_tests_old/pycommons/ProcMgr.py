# $Id$
# Artem V. Nikitin
# (sub)process management classes

from Logger import log, logException
#from Util import *
from threading import Thread, Event
import subprocess, signal, os, time, string


# class to process output from a process asynchronously
class BindOutput:

    def __init__( self, name, file, fn, fnArgs = (), finishFn = None, finArgs = () ):
        self.name = name
        self.file = file
        self.fn = fn
        self.fnArgs = fnArgs
        self.finishFn = finishFn
        self.finArgs = finArgs
        self.thread = Thread(None, self._run, '%s' % self.name)
        self.thread.start()

    def _run( self ):
        try:
            while 1:
                line = self.file.readline()
                if not line:
                    if self.finishFn:
                        try:
                            apply(self.finishFn, self.finArgs)
                        except:
                            logException(1)
                    break
                if line[-1] == '\n': line = line[:-1]
                try:
                    apply(self.fn, (line,) + self.fnArgs)
                except:
                    logException(1)
        except:
            logException(1)

    def close( self ):
        self.thread.join()


# Class to manage (start, stop and log output) a process.
# 'start' must be called from the main thread, so that signals for subprocess work.
class Process:

    def __init__( self, name, cmd, procMgr = None, env = None ):
        self.__name = name
        self.__cmd = cmd
        self.__procMgr = procMgr
        self.__env = env
        if self.__procMgr:
            self.__procMgr.add(self)
        self.__out = None
        self.__err = None

    @property
    def name( self ):
        return self.__name

    def start( self ):
        log(2, 'starting "%s"' % self.__cmd)
        self.__proc = subprocess.Popen(
          self.__cmd,
          stdout = subprocess.PIPE,
          stderr = subprocess.PIPE,
          env = self.__env)

        self.__out = BindOutput(self.__name, self.__proc.stdout, self._output, (), self._finished)
        self.__err = BindOutput(self.__name, self.__proc.stderr, self._error)

    # will block until process finishes
    def _close( self ):
        if self.__out: self.__out.close()
        if self.__err: self.__err.close()

    def _output( self, line ):
        log(5, '%s' % line)

    def _error( self, line ):
        log(1, '[ERROR] %s' % line)

    def _finished( self ):
        log(3, '%s finished' % self.__name)

    def _kill( self, sig = signal.SIGTERM ):
        if self.__proc.poll() is None:
            log(13, 'sending signal %d to %s (%d)' % (sig, self.__name, self.__proc.pid))
            os.kill(self.__proc.pid, sig)
            finishTime = time.time() + 5.0
            while self.__proc.poll() is None and time.time() < finishTime:
                time.sleep(0.5)
            if self.__proc.poll() is None:
                log(13, 'sending signal %d to %s (%d)' % (signal.KILL, self.__name, self.__proc.pid))
                os.kill(self.__proc.pid, signal.KILL)
        else:
            log(13, 'not sending signal %d to %s - already stopped' % (sig, self.__name))

    def stop( self ):
        try:
            self._kill()
        except:
            pass
        self._close()


# Class to manage all subprocesses. Must be run'ned in main thread
class ProcMgr:

    def __init__( self ):
        self._processes = []

    def add( self, proc ):
        self._processes.append(proc)

    def start( self ):
        for proc in self._processes:
            if proc.startOnStart:
                proc.start()

    def close( self ):
        log(11, 'stopping subprocesses...')
        for proc in self._processes:
            log(12, 'stopping %s' % proc.name)
            proc.stop()
            log(12, '%s is stopped' % proc.name)
        log(11, 'stopping subprocesses: done')
