#!/usr/bin/env python
# -*- coding: utf-8 -*-
__author__ = 'Danil Lavrentyuk'
""" Server stress test scrtipt. Trying to make large number of requests to see if
"""
import sys, os, threading
import argparse
import requests
from requests.exceptions import SSLError, ConnectionError, RequestException
import signal
import time
import random
import traceback as TB
from collections import deque
from subprocess import Popen, PIPE

DEFAULT_THREADS = 10
AUTH = ('admin', 'admin')
URI = "/api/gettime" # "/ec2/getCurrentTime", /api/moduleInformation, /api/ping, /api/statistics,
# /ec2/getSettings, /api/moduleInformation, /api/logLevel, /api/iflist, /api/systemSettings
URI_HEAVY = "/ec2/getResourceTypes" # /ec2/getFullInfo
DEFAULT_HOST = "192.168.109.12"
DEFAULT_PORT = 7001
HOST = ""
PROTO = 'http'
REPORT_PERIOD = 1.0
BATCH_PERIOD = 45
FAILS_TAIL = 10 # Now many previous REPORT_PERIODs data to hold
KILL_REQ_AFTER = 0.003
INTFLOOD_PERIOD = 30.0 # seconds, interrupted requests flood duration
HANG_GRACE = 5.0 # in batch mode, if there was no successes for this peroid then the server is hung/crashed

logfile = None

def mk_url(proto, host, uri):
    return "%s://%s%s" % (proto, host, uri)


def _init_fails():
    return {None: 0}


def dict_add(accum, src):
    for k, v in src.iteritems():
        if k in accum:
            accum[k] += v
        else:
            accum[k] = v

def dict_sub(accum, src):
    for k, v in src.iteritems():
        if k in accum:
            accum[k] -= v
        else:
            accum[k] = -v


class BaseWorker(object):

    def __init__(self, master, num):
        self._id = num
        self._master = master  # type: StressTestRunner
        self.fails = _init_fails() # failures, groupped by error message

    def getId(self):
        return "%s.%d" % (self.__class__.__name__, self._id)

    def _report(self, result):
        if result in self.fails:
            self.fails[result] += 1
        else:
            self.fails[result] = 1

    def _output(self, msg):
        if self._master.need_logexc():
            sys.stdout.write(msg)
        if logfile:
            print >>logfile, msg

    def _req(self):
        raise NotImplementedError()

    def run(self):
        while not self._master.stopping():
            self._report(self._req())


class RequestWorker(BaseWorker):
    """ Produces many requsests to the server.
    """
    def __init__(self, master, num, mix):
        super(RequestWorker, self).__init__(master, num)
        self.fails = _init_fails() # failures, groupped by error message
        self._session = requests.Session()
        if mix:
            self._url = None
            self._prep = [
                self._session.prepare_request(requests.Request('GET', url=mk_url(proto, HOST, uri), auth=AUTH))
                for proto in ('http', 'https')
                for uri in (URI, URI_HEAVY)
            ]
        else:
            self._url = mk_url(PROTO, HOST, URI)
            kwargs = dict(url=self._url, auth=AUTH)
            self._prep = [self._session.prepare_request(requests.Request('GET', **kwargs))]

    def _req(self):
        try:
            req = random.choice(self._prep)
            kwargs = {'verify': False} if req.url.startswith('https') else dict()
            if logfile:
                print >>logfile, "URL: %s, HTTPS %s" % (req.url, 'ON' if 'verify' in kwargs else 'OFF')
            res = self._session.send(req,   **kwargs)
            if res.status_code == 200:
                return None # means success!
            else:
                return 'Code: %s' % res.status_code
        except RequestException, e:
            self._output("%s: %s\n" % (type(e).__name__, e.message))
            return type(e).__name__
        except Exception, e:
            self._output("Exception: %s\n" % (TB.format_exc(),))
            return type(e).__name__


class InteruptingWorker(BaseWorker):

    err = 'curl: (7) Failed to connect'

    def __init__(self, master, num):
        super(InteruptingWorker, self).__init__(master, num)
        self._urls = [mk_url("https", HOST, uri) for uri in (URI, URI_HEAVY)]

    def _req(self):
        cmd = ['/usr/bin/env', 'curl',
              '--insecure',
              '--fail', # returns error code 22 in most cases of http errors
              '--user', "%s:%s" % AUTH,
              random.choice(self._urls)]
        #print "Command: %s" % (' '.join(cmd),)
        fnull = open(os.devnull, 'w')
        try:
            proc = Popen(cmd, stderr=fnull, stdout=fnull, shell=False)
            time.sleep(KILL_REQ_AFTER)
            while proc.poll() is None:
                proc.kill()
            while proc.poll() is None:
                time.sleep(0.001)
            if proc.returncode is None:
                #print "*** CURL'S DOING TO LONG ***"
                os.kill(proc.pid, signal.SIGTERM)
                if proc.poll() is None:
                    time.sleep(0.5)
                    proc.poll()
            #elif proc.returncode == -9:
            #    time.sleep(1)
            #print "Retcode: %s" % (proc.returncode,)
            if proc.returncode != 0:
                return "RC=%s" % (proc.returncode,)
            return None
        except Exception, e:
            self._output("Exception: %s\n" % (TB.format_exc(),))
            return type(e).__name__


class StressTestRunner(object):
    """ Hold a container of test runners and runs them in threads.
    """
    _stop = False
    _threadNum = DEFAULT_THREADS

    def __init__(self, args):
        self._threads = []
        self._workers = []
        self._fails = _init_fails() # failures, groupped by error message
        self._threadNum = args.threads
        self._mix = args.mix
        self._drop = args.drop
        self._batch = args.batch
        self._tail = deque(maxlen=FAILS_TAIL+2) # because the first is the base value, and there is also one more final record
        self._hang = False
        self._nostat = False
        self._full = args.full
        self._logexc = args.logexc
        signal.signal(signal.SIGINT,self._onInterrupt)

    def need_logexc(self):
        return self._logexc

    def _createWorkers(self):
        if self._drop:
            self._workers = [InteruptingWorker(self, n) for n in xrange(self._threadNum)]
        else:
            self._workers = [RequestWorker(self, n, self._mix) for n in xrange(self._threadNum)]

    def _onInterrupt(self, _signum, _stack):
        self._stop = True
        print "Finishing work..."

    def stopping(self):
        return self._stop

    def report(self, result):
        if result in self._fails:
            self._fails[result] += 1
        else:
            self._fails[result] = 1

    def _collect_stat(self):
        """Collects success and error counters from workers."""
        passed = time.time() - self._start
        fails = _init_fails()
        for w in self._workers:
            dict_add(fails, w.fails)
        if not self._nostat:
            self._tail.append((passed, fails))
        return passed, fails


    @staticmethod
    def _stat_str(passed, fails):
        total = sum(fails.itervalues())
        oks = fails[None]
        return "Success %d/%d (%.1f%%), avg. %.1f req/sec.%s" % (oks, total,
            ((oks * 100.0)/total) if total else 0, (total/passed) if passed else 0,
            ("\nFails: " + ', '.join("%s %s" % (k, v) for k, v in fails.iteritems() if k is not None)) if len(fails) > 1 else ''
        )

    def print_totals(self):
        passed, fails = self._collect_stat()
        if self._nostat:
            print "[%s] %d\n" % (int(round(passed)), sum(fails.itervalues())),
        else:
            print "[%s] %s\n" % (int(round(passed)), self._stat_str(passed, fails)),

    def print_tail(self):
        tail_start, fails_a = self._tail[0]
        tail_end, fails = self._tail[-1]
        dict_sub(fails, fails_a)
        print "Last %.1f seconds: %s" % (tail_end - tail_start, self._stat_str(tail_end - tail_start, fails))

    def _check_server_hung(self):
        passed, fails = self._tail[-1]
        found = False
        for i in reversed(xrange(len(self._tail)-1)):
            if passed - self._tail[i][0] > HANG_GRACE:
                found = True
                break
        if not found:
            return False
        if fails[None] == 0 or fails[None] == self._tail[i][1][None]: # i.e. there was no more successes after i's
            print "FAIL: No successes for the last %.1f seconds!" % (passed - self._tail[i][0],)
            self._hang = True
            return True
        return False

    def _startThreads(self):
        self._threads = [threading.Thread(target=w.run, name=w.getId()) for w in self._workers]
        self._start = time.time()
        for t in self._threads:
            t.daemon = True
            t.start()

    def _joinThreads(self):
        for t in self._threads:
            if t.isAlive():
                t.join()

    def _watchThreads(self, end_time):
        period = REPORT_PERIOD / 5.0
        while not self._stop:
            mark = time.time() + REPORT_PERIOD
            while time.time() < mark:
                time.sleep(period)
            self.print_totals()
            if (not self._drop) and self._check_server_hung():
                self._stop = True
            if end_time is not None and end_time < time.time():
                self._stop = True

    def _reinit(self):
        self._stop = False
        self._hang = False
        self._fails = _init_fails()
        self._tail.clear()

    def _drop_stress(self):
        self._drop = True
        self._nostat = True
        self._createWorkers()
        self._startThreads()
        self._watchThreads(self._start + INTFLOOD_PERIOD)
        del self._workers[:]
        self._drop = False
        self._nostat = False

    def run(self):
        if self._full is not None:
            self.full_test()
        else:
            self.simple_test()

    def simple_test(self):
        print "Testing with %s parallel workers. %s" % (
            self._threadNum,
            "Batch mode for %s seconds" % self._batch if self._batch is not None else
            ("Mix-mode" if self._mix else ("Protocol: %s" % PROTO))
        )
        self._createWorkers()
        self._startThreads()
        self._watchThreads(self._start + self._batch if self._batch is not None else None)
        self._joinThreads()
        print "===========================\nFinal result:"
        self.print_totals()
        if not self._nostat:
            self.print_tail()
        if not self._hang:
            self._check_server_hung()
        if self._batch is not None:
            print "FAIL:" if self._hang else "OK"
        del self._workers[:]

    def full_test(self):
        if self._full == "":
            steps = (BATCH_PERIOD/3, 2*BATCH_PERIOD/3, BATCH_PERIOD)
        else:
            steps = [int(s) for s in self._full.split(',')]
            if len(steps) < 2:
                steps.append(steps[0]*2)
            if len(steps) < 3:
                steps.append(steps[0]*3)
        self._batch = steps[0]
        print "Test 1: Normal requests."
        self.simple_test()
        if not self._hang:
            print "Test 2: Flood of interrupted requests, than try some nornal ones."
            self._reinit()
            self._batch = steps[1]
            self._drop_stress()
            self._reinit()
            self._batch = steps[2]
            self.simple_test()
        print 'Test complete.'


def parse_args():
    global PROTO, REPORT_PERIOD, HOST, URI
    parser = argparse.ArgumentParser()
    parser.add_argument('-T', '--threads', type=int, help="Number of threads to use. Default: %s" % DEFAULT_THREADS, default=DEFAULT_THREADS)
    parser.add_argument('-H', '--host', help="Server hostname or IP. May include portnumber too.", default=DEFAULT_HOST)
    parser.add_argument('-P', '--port', type=int, help="Server port number", default=DEFAULT_PORT)
    parser.add_argument('-p', '--proto', choices=('http', 'https'), help="Protocol using (http or https, %s is default)" % PROTO)
    parser.add_argument('-r', '--reports', type=float, help="Counters report preriod, seconds. Default = %.1f" % REPORT_PERIOD)
    parser.add_argument('-y', '--heavy', action='store_true', help="Use requests with heavy response")
    parser.add_argument('-x', '--mix', action='store_true', help="Mix mode: http and https, light and heavy requests")
    parser.add_argument('-l', '--log', action='store_true', help="Write a log file")
    parser.add_argument('-b', '--batch', type=int, help="Batch testing mode. Optional value - testing duration, whole seconds. Default is %s" % BATCH_PERIOD, nargs='?', const=BATCH_PERIOD)
    parser.add_argument('-f', '--full', help="Full 2-step test with drop-phase", nargs='?', const="")
    parser.add_argument('-e', '--logexc', action='store_true', help="Log to console every request exception. Without it exceptions are counted only")
    #TODO: add print exception option (don't print request exceptions withot it)
    #TODO: add option to set HANG_GRACE and calculate the minimal FAILS_TAIL from it
    #TODO: also check that HANG_GRACE is less than BATCH_PERIOD
    #TODO: add quiet mode option to produce less outpot
    parser.add_argument('--drop', action='store_true', help="Debug request dropping")
    args = parser.parse_args()
    #if args.host and args.port:
    if args.reports is not None:
        if args.reports > 0:
            REPORT_PERIOD = args.reports
        else:
            print "ERROR: Wrong report period value: %s" % args.reports
            sys.exit(1)
    if args.batch is not None or args.full is not None:
        args.mix = True
    if args.mix and (args.heavy or args.proto):
        print "ERROR: --proto and --heavy options meaningless in the mix, batch and full modes."
        sys.exit(1)
    host = args.host.split(':', 1)
    if len(host) > 1:
        if host[1] != str(args.port):
            print "ERROR: Hostname contains a port number %s and it's not equal to the option --port value %s" % (host[1], args.port)
            sys.exit(1)
        # else no changes to args.host required
    else:
        args.host = '%s:%s' % (host[0], args.port)
    HOST = args.host
    if args.proto:
        PROTO = args.proto
    if args.heavy:
        print "Using heavy requests"
        URI = URI_HEAVY
    return args


if __name__ == '__main__':
    args = parse_args()
    if args.log and args.mix:
        fname = "stresst.log"
        logfile = open("stresst.log","wt")
        print "Writting logfile %s" % fname
    StressTestRunner(args).run()
    if logfile:
        logfile.close()
