#!/usr/bin/env python
# $Id$
# Artem V. Nikitin
# Performance test for the media server

import time, sys
sys.path.append('..')
from pycommons.Logger import logException, log, initLog, LOGLEVEL
import pycommons.MockClient as clientlib

from multiprocessing import Process, Queue, Event, current_process, Lock, Value
from optparse import OptionParser
from ctypes import *
from Queue import Empty

DEFAULT_PROCESSES=10
DEFAULT_SAMPLE_SIZE=10000
DEFAULT_LOG_LEVEL=3
DEFAULT_TIMEOUT=1.0

class ShuttingDown(Exception): pass

class SingleProcess(object):

    def __init__( self, procName, target = None, args = () ):
        self.procName = procName
        self.target = target  # run method
        self.args   = args
        self.proc = Process(name=self.procName, target=self._run)

    def start( self ):
        # TODO. Need more clear way to tune base logger
        import pycommons.Logger as logger
        logger.getThreadName = self.getThreadName
        self.proc.start()

    def terminate( self ):
        self.proc.terminate()

    def close( self ):
        log(LOGLEVEL.INFO, 'waiting for %s process...' % self.procName)
        self.proc.join()
        log(LOGLEVEL.INFO, '%s process stopped' % self.procName)

    def getThreadName(self):
        return current_process().name

    def _run( self ):
        log(LOGLEVEL.INFO, ' started')
        try:
            self.run()
            log(LOGLEVEL.INFO, 'process finished')
        except:
            self.failed()

    def failed( self ):
        logException()
        log(LOGLEVEL.ERROR, 'proc failed')
        raise ShuttingDown("ShutDown")
    
    def run( self ):
        assert self.target, "Process 'run' method is not overloaded and 'target' is not passed"
        self.target(*self.args)

class Task(object):
  
    def execute( self, *args ):
        methodNotImplemented()

class StopTask(Task):
        
    def execute( self, *args ):
        stopEvent = args[0]
        stopEvent.set()

class ProcessPool(object):

    def __init__( self, name, procCount, args = ()):
        assert procCount > 0, 'there must be at least one process in the pool'
        self.name = name
        self.procCount = procCount
        self.processes = []
        self.taskQueue = Queue()
        self.stopEvent = Event()
        for i in range(self.procCount):
            self.processes.append(SingleProcess('%s#%d' % (self.name, i), self._run, args=(self.stopEvent,) + args))

    def start( self ):
        for proc in self.processes:
            proc.start()

    def close( self ):
        """Wait for all processes to exit."""
        assert self.processes, '%s is not started yet' % self.name
        log(LOGLEVEL.INFO, '%s: waiting for processes...' % self.name)
        self.taskQueue.close()
        for proc in self.processes:
            proc.close()
        log(LOGLEVEL.INFO, '%s: stopped.' % self.name)
            
    def stop( self ):
        if not self.stopEvent.is_set():
            self.execute(StopTask())
            try:
                self.stopEvent.wait()
            except:
                # It's possible keybord interruption
                self.terminate()
        self.close()

    def terminate(self):
        for proc in self.processes:
            proc.terminate()

    def execute( self, task ):
        self.taskQueue.put(task)

    def _run( self, *args ):
        stopEvent = args[0]
        while not stopEvent.is_set():
            try:
                task = self.taskQueue.get(False, 0.000001)
                #task = self.taskQueue.get()
                log(LOGLEVEL.DEBUG, 'executing task %s...' % task)
                task.execute(*args)
                log(LOGLEVEL.DEBUG, 'executing task %s done.' % task)
            except Empty:
                pass
            except ShuttingDown:
                log(LOGLEVEL.ERROR, 'executing task failed.')
                stopEvent.set()
                break


class RequestTask(Task):

    def __init__(self, options):
        self.server = options.address
        self.request = options.request
        self.user = options.user
        self.password = options.password
        self.digest = options.digest
        self.basic = options.basic

    def getClient(self):
        if self.digest:
            return clientlib.DigestAuthClient(
                user = self.user,
                password = self.password,
                timeout = DEFAULT_TIMEOUT)
        elif self.basic:
            return clientlib.BasicAuthClient(
                user = self.user,
                password = self.password,
                timeout = DEFAULT_TIMEOUT)
        return clientlib.Client(timeout = DEFAULT_TIMEOUT)
   
    def execute(self, *args):
        stopEvent, stat = args
        client = self.getClient()
        
        stat.reqCount.incr()
        response = client.httpRequest(self.server, self.request)
        if not response or response.status < 200 or response.status > 299:
            stat.non2XX.incr()
        elif isinstance(response, client.ServerResponseData):
            stat.bytes.incr(len(response.rawData))
            if type(response.data) is str:
                stat.nonJson.incr()
            elif not isinstance(response.data, list):
                error = int(response.data.get('error', '0'))
                errorString = response.data.get('errorString', '')
                if error or errorString: stat.apiErrors.incr()

               
class Counter(object):

    def __init__(self):
        self._val = Value(c_longlong, 0)

    def incr(self, val = 1):
        with self._val.get_lock():
            self._val.value+=val
        
    def value(self):
        with self._val.get_lock():
            return self._val.value
           
        
class Statistics(object):

    def __init__(self, reqCount, errors, non2XX, apiErrors, nonJson, bytes):
        self.reqCount = reqCount
        self.errors = errors
        self.non2XX = non2XX
        self.apiErrors = apiErrors
        self.nonJson = nonJson
        self.bytes = bytes

def main():
    usage = "usage: %prog [options]"
    parser = OptionParser(usage)
    parser.add_option("-a", "--address",
                      help="Tested server address, for example 127.0.0.1:7001.")

    parser.add_option("-r", "--request",
                      help="Test request, for example ec2/testConnection.")

    parser.add_option("--user", default = clientlib.DEFAULT_USER,
                      help="Media server username.")

    parser.add_option("--password", default = clientlib.DEFAULT_PASSWORD, 
                      help="Media server user password.")

    parser.add_option("--digest", default=False, action="store_true",
                    help="Use digest authorization for test client.")

    parser.add_option("--basic", default=False, action="store_true",
                    help="Use basic authorization for test client.")
        
    parser.add_option("-c", "--concurency", type="int", default = DEFAULT_PROCESSES,
                      help="Concurency process number.")
    
    parser.add_option("-s", "--size", type="int", default = DEFAULT_SAMPLE_SIZE,
                      help="Total requests count.")

    parser.add_option("--logLevel", type="int", default = DEFAULT_LOG_LEVEL,
                      help="Log level.")

    parser.add_option("--log", help="Log file.")

    
    (options, args) = parser.parse_args()

    
    if not options.address:
        parser.print_help()
        parser.error('Server address is required.')

    if not options.request:
        parser.print_help()
        parser.error('Test request is required.')

    if options.digest and options.basic:
        parser.print_help()
        parser.error("You can use only one option ('digest' or 'basic') at the same time.")

    initLog(options.logLevel, options.log)


    stat = Statistics(
        Counter(),
        Counter(),
        Counter(),
        Counter(),
        Counter(),
        Counter())
   
    pool = ProcessPool('RequestPool', options.concurency, args = (stat,))
    pool.start()
    start = time.time()

    for i in xrange(options.size):
        pool.execute(
            RequestTask(options))

    pool.stop()

    duration = time.time() - start
    print "============================================================="
    print "Requests: %d" % stat.reqCount.value()
    print "Bytes: %d" % stat.bytes.value()
    print "Errors: %d" % stat.errors.value()
    print "Non2xx: %d" % stat.non2XX.value()
    print "NonJSON: %d" % stat.nonJson.value()
    print "API errors: %d" % stat.apiErrors.value()
    print "Duration: %.2f seconds" % duration
    print "Requests per second: %.2f" % (stat.reqCount.value() / duration)
    print "Bytes per second: %.2f" % (stat.bytes.value() / duration)
    print "============================================================="

if __name__ == '__main__':
    main()
