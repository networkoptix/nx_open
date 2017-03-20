# -*- coding: utf-8 -*-
__author__ = 'Danil Lavrentyuk'
import urllib2
import unittest
import subprocess
import traceback
import time
import os
import json
import socket
import struct
import pprint
import sys
from pycommons.Utils import secs2str
from pycommons.Logger import log, LOGLEVEL

from testbase import *

GRACE = 1.0 # max time difference between responses to say that times are equal
INET_GRACE = 3.0 # max time difference between a mediaserver time and the internet time
DELTA_GRACE = 0.5 # max difference between two deltas (each between a mediaserver time and this script local time)
                   # used to check if the server time hasn't changed
SERVER_SYNC_TIMEOUT = 10 # seconds
MINOR_SLEEP = 1 # seconds
SYSTEM_TIME_SYNC_SLEEP = 10.5 # seconds, a bit greater than server's systime check period (10 seconds)
SYSTEM_TIME_NOTSYNC_SURE = 30 # seconds, how long wait to shure server hasn't synced with system time
INET_SYNC_TIMEOUT = 15 * 4 # bigger than the value in ctl.sh in prepare_isync case

IF_EXT = 'eth0'

class TimeTestError(FuncTestError):
    pass

#time_servers = ('time.nist.gov', 'time.ien.it') # 'time-nw.nist.gov')
time_servers = ('instance1.rfc868server.com', )
TIME_PORT = 37
TIME_SERVER_TIMEOUT = 10
SHIFT_1900_1970 = 2208988800

def get_inet_time(host):
    s = None
    try:
        s = socket.create_connection((host, TIME_PORT), TIME_SERVER_TIMEOUT)
        d = s.recv(4)
        return d
    except Exception:
        return ''
    finally:
        if s:
            s.close()

def check_inet_time():
    "Make sure that at least one of time servers, used by mediaserver for time synchronization, is available and responding."
    for _ in xrange(3):  # try several times
        for host in time_servers:
            d = get_inet_time(host)
            if len(d) == 4:
                return d # The first success means OK
        time.sleep(0.5)
    return False

EMPTY_TIME = dict(
    value=0,
    time=0,
    local=0,
    boxtime=0,
    isPrimaryTimeServer=False,
    primaryTimeServerGuid='',
    syncTimeFlags='',
)

###########
## NOTE:
## Really it's a sequence of tests where most of tests depend on the previous test result.
##

class TimeSyncTest(FuncTestCase):
    #num_serv = _NUM_SERV # override
    helpStr = "Time synchronization tests"
    _suites = (
        ('SyncTimeNoInetTests', [
            'InitialSynchronization',
            'ChangePrimaryServer',
            'PrimarySystemTimeChange',
            'SecondarySystemTimeChange',
            'StopPrimary',
            'RestartSecondaryWhilePrimaryOff',
            'StartPrimary',
            'PrimaryStillSynchronized',
            'MakeSecondaryAlone'
        ]),
        ('InetTimeSyncTests', [
            'TurnInetOn',
            'ChangePrimarySystime',
            'KeepInetTimeAfterIfdown',
            'KeepInetTimeAfterSecondaryOff',
            'KeepInetTimeAfterSecondaryOn',
            'KeepInetTimeAfterRestartPrimary',
            'BothRestart_SyncWithOS',
            'PrimaryFollowsSystemTime',
        ])
    )
    _test_name = "TimeSync"
    _test_key = 'timesync'
    _init_time = []
    _primary = None
    _secondary = None
    times = []

    @classmethod
    def setUpClass(cls):
        if cls.testset == 'InetTimeSyncTests':
            if not check_inet_time():
                raise AssertionError("Internet time servers aren't accessible")
        super(TimeSyncTest, cls).setUpClass()
        if not cls._init_time:
            t = int(time.time())
            cls._init_time = [str(t - v) for v in (72000, 144000)]

    @classmethod
    def tearDownClass(cls):
        log(LOGLEVEL.INFO, "Restoring external interfaces")
        #FIXME: use threads and subprocess.check_output, tbe able to log stderr messages
        with open(os.devnull, 'w') as FNULL:
            proc = [
                #FIXME make the interface name the same with $EXT_IF fom conf.sh !!!
                (box, subprocess.Popen(
                    ['./vssh-ip.sh', box, 'sudo', 'ifup', IF_EXT], shell=False, stdout=FNULL, stderr=subprocess.STDOUT)
                 ) for box in cls.hosts
            ]
            for b, p in proc:
                if p.wait() != 0:
                    log(LOGLEVEL.ERROR, "ERROR: Failed to start external interface for box %s" % (b,))
        super(TimeSyncTest, cls).tearDownClass()

    def setUp(self):
        self.times = [EMPTY_TIME.copy() for _ in xrange(self.num_serv)]

    @classmethod
    def _global_clear_extra_args(cls, num):
        return (str(int(time.time())),)

#    @classmethod
#    def globalFinalise(cls):
#        for num in xrange(cls.num_serv):
#            cls.class_call_box(cls.hosts[num], 'date', '--set=@%d' % int(time.time()))
#            #cls.class_call_box(cls.hosts[num], '/vagrant/ctl.sh', 'timesync', 'clear', str(int(time.time())))
#        super(TimeSyncTest, cls).globalFinalise()


    ################################################################

    def _init_script_args(self, boxnum):
        return (self._init_time[boxnum],)

    @classmethod
    def _global_clear_extra_args(cls, num):
        return (str(int(time.time())),)

    def _show_systime(self, box):
        log(LOGLEVEL.DEBUG + 4, "OS time for %s: %s " % (box, self.get_box_time(box)))

    def debug_systime(self):
        for box in self.hosts:
            self._worker.enqueue(self._show_systime, (box,))

    def _request_gettime(self, boxnum, ask_box_time=True):
        "Request server's time and its system time. Also return current local time at moment response was received."
#        answer = self._server_request(boxnum, 'api/gettime',)
        answer = self._server_request(boxnum, 'ec2/getCurrentTime')
        answer['time'] = int(answer['value']) / 1000.0
        answer['local'] = time.time()
        answer['boxtime'] = self.get_box_time(self.hosts[boxnum]) if ask_box_time else 0
        log(LOGLEVEL.DEBUG + 9, "Server#%d ec2/getCurrentTime time: %s" %
            (boxnum, secs2str(answer['time'])))
        return answer

    def _task_get_time(self, boxnum):
        "Call _request_gettime and store result in self.times - used tu run in a separate thread"
        self.times[boxnum] = self._request_gettime(boxnum)

    def _check_time_sync(self, sync_with_system=True):
        end_time = time.time() + SERVER_SYNC_TIMEOUT
        while time.time() < end_time:
            reason = ''
            time.sleep(0.2)
            for boxnum in xrange(self.num_serv):
                self._worker.enqueue(self._task_get_time, (boxnum,))
            self._worker.joinQueue()
            delta = self.times[0]['local'] - self.times[1]['local']
            diff = self.times[0]['time'] - self.times[1]['time'] + delta
            if abs(diff) > GRACE:
                reason = "Time difference too high: %.3f" % diff
                continue
            elif not sync_with_system:
                return
            if sync_with_system:
                td = [abs(t['boxtime'] - t['time']) for t in self.times]
                min_td = min(td)
                if min_td <= GRACE:
                    type(self)._primary = td.index(min_td)
                    if not self.before_2_5:
                        self.assertTrue(self.times[self._primary]['isPrimaryTimeServer'],
                            "Time was syncronized by server %s system time, " \
                               "but it's isPrimaryTimeServer flag is False, timedeltas: '%s'" % (self._primary, td))
                    log(LOGLEVEL.INFO, "Synchronized by box %s" % self._primary)
                    return
                else:
                    reason = "None of servers report time close enough to it's system time. Min delta = %.3f" % min_td
        #self.debug_systime()
        log(LOGLEVEL.ERROR, "0: %s" % self.times[0])
        log(LOGLEVEL.ERROR, "1: %s" % self.times[1])
        self.fail(reason)
        #TODO: Add more details about servers' and their systems' time!

    def _check_systime_sync(self, boxnum, must_synchronize=True):
        tt = time.time() + (1 if must_synchronize else SYSTEM_TIME_NOTSYNC_SURE)
        timediff = 0
        log(LOGLEVEL.DEBUG + 9, "DEBUG: now %s, checking for sync until %s" % (int(time.time()), int(tt)))
        while time.time() < tt:
            timedata = self._request_gettime(boxnum)
            log(LOGLEVEL.DEBUG + 9, "DEBUG: server time: %s, system time: %s" % (timedata['time'], timedata['boxtime']))
            timediff = abs(timedata['time'] - timedata['boxtime'])
            if timediff > GRACE:
                if must_synchronize:
                    log(LOGLEVEL.DEBUG + 9, "DEBUG (_check_systime_sync): %s server time: %s, system time: %s. Diff = %.2f - too high" % (
                        boxnum, timedata['time'], timedata['boxtime'], timediff))
                time.sleep(1)
            else:
                break
        if must_synchronize:
            self.assertLess(timediff, GRACE, "Primary server time hasn't synchronized with it's system time change. Time difference = %s" % timediff)
        else:
            log(LOGLEVEL.ERROR, "Time diff = %.1f, grace = %s" % (timediff, GRACE))
            self.assertGreater(timediff, GRACE, "Secondary server time has synchronized with it's system time change")

    def get_box_time(self, box):
        resp = self._call_box(box, 'date', '+%s')
        # Get last line of output to get rid of ssh output
        return int(resp.splitlines()[-1].rstrip())

    def shift_box_time(self, box, delta):
        "Changes OS time on the box by the shift value"
        self._call_box(box, "/vagrant/chtime.sh", str(delta))

    def _serv_local_delta(self, boxnum):
        """Returns delta between mediasever's time and local script's time
        """
        times = self._request_gettime(boxnum) #, False) -- return this after removing the next debug
        self.times[boxnum] = times
        #print "DEBUG: %s server time %s, system time %s, local time %s" % (boxnum, times['time'], times['boxtime'], times['local'])
        #print "!DEBUG: %s server times: " % boxnum,
        #pprint.pprint(times)
        return times['local'] - times['time']  # local time -- server's time

    def _get_secondary(self):
        "Return any (next) server number which isn't the primary server"
        return self.num_serv - 1 if self._primary == 0 else self._primary - 1

    def _setPrimaryServer(self, boxnum):
        log(LOGLEVEL.INFO, "Force box %s (%s) to be the primary server" % (boxnum, self.hosts[boxnum]))
        self._server_request(boxnum, 'ec2/forcePrimaryTimeServer', data={'id': self.guids[boxnum]})

    ####################################################

    def InitialSynchronization(self):
        """Stop both mediaservers, initialize boxes' system time and start the servers again. Check for sync.
        """
        self._prepare_test_phase(self._stop_and_init)
        #self.debug_systime()
        log(LOGLEVEL.INFO, "Checking time...")
        self._check_time_sync()
        #self.debug_systime()
        #TODO add here flags check, what server has became the primary one

    def ChangePrimaryServer(self):
        """Check mediaservers' time synchronization by the new primary server.
        """
        #self.debug_systime()
        self._setPrimaryServer(self._get_secondary())
        time.sleep(MINOR_SLEEP)
        old_primary = self._primary
        self._check_time_sync()
        #self.debug_systime()
        self.assertNotEqual(old_primary, self._primary, "Primary server hasn't been changed")
        #TODO add the flags check!

    def PrimarySystemTimeChange(self):
        """Change system time on the primary servers' box, check if the servers have synchronized.
        """
        self.shortDescription()
        #self.debug_systime()
        primary = self._primary # they'll be compared later
        box = self.hosts[primary]
        log(LOGLEVEL.INFO, "Use primary box %s (%s)" % (primary, box))
        self.shift_box_time(box, 12345)
        #self.debug_systime()
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)
        self._check_systime_sync(primary)
        self._check_time_sync()
        self.assertEqual(primary, self._primary, "Time was synchronized by secondary server")

    #@unittest.skip("Debugging....")
    def SecondarySystemTimeChange(self):
        """Change system time on the secondary servers' box, check if the servers have ignored it.
        """
        primary = self._primary # they'll be compared later
        sec = self._get_secondary()
        box = self.hosts[sec]
        log(LOGLEVEL.INFO, "Use secondary box %s (%s)" % (sec, box))
        self.shift_box_time(box, -12345)
        #self.debug_systime()
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)
        self._check_systime_sync(sec, False)
        self._check_time_sync()
        self.assertEqual(primary, self._primary, "Primary server has been changed")

    def StopPrimary(self):
        """Check if stopping the primary server doesn't lead to the secondary's time change.
        """
        type(self)._secondary = self._get_secondary()
        #self._show_systime(self.hosts[self._primary])
        delta_before = self._serv_local_delta(self._secondary) # remember difference between seco9ndary server's time and the local time
        self._mediaserver_ctl(self.hosts[self._primary], 'stop')
        time.sleep(MINOR_SLEEP)
        delta_after = self._serv_local_delta(self._secondary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="The secondary server's time changed after the primary server was stopped")

    #@unittest.expectedFailure
    def RestartSecondaryWhilePrimaryOff(self):
        """Check if restarting the secondary (while the primary is off) doesn't change it's time
        """
        delta_before = self._serv_local_delta(self._secondary)
        log(LOGLEVEL.INFO, "Delta before = %.2f" % delta_before)
        self._mediaserver_ctl(self.hosts[self._secondary], 'restart')
        log(LOGLEVEL.INFO, "Waiting for secondary restarted")
        #try:
        self._wait_servers_up(set((self._secondary,)))
        #except Exception, e:
        #    print "_wait_servers_up() fails with exception:"
        #    traceback.print_exc()
        #    raise
        #print "Check...."
        delta_after = self._serv_local_delta(self._secondary)
        log(LOGLEVEL.INFO, "Delta after = %.2f" % delta_after)
        time.sleep(0.1)
        if not self.before_2_5:
            self.assertFalse(self.times[self._secondary]['isPrimaryTimeServer'],
                             "The secondary server (%s) has become the primary one" % self._secondary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="The secondary server's time changed (by %.3f) after it was restarted (while the primary is off)" %
                                   (delta_before - delta_after))

    def StartPrimary(self):
        """Check if starting the primary server again doesn't change time on the both primary and secondary.
        """
        delta_before = self._serv_local_delta(self._secondary)
        self._mediaserver_ctl(self.hosts[self._primary], 'start')
        self._wait_servers_up()
        delta_after = self._serv_local_delta(self._secondary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="The secondary server's time changed after the primary server was started again")
        #self.debug_systime()
        self._check_time_sync(False)

    def PrimaryStillSynchronized(self):
        self._check_systime_sync(self._primary)

    #TODO: now it should test thet the secondary DOES'T become the new primary and DOESN'T sync. with it's OS time
    @unittest.skip("To be reimpemented")
    def MakeSecondaryAlone(self):
        """Check if the secondary after stopping and deleting the primary becomes primary itself and synchronize with it's OS time.
        """
        self._mediaserver_ctl(self.hosts[self._primary], 'stop')
        box = self.hosts[self._secondary]
        self.shift_box_time(box, -12345)
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)

        # remove from the secondary server the record about the prime (which is down now)
        self._server_request(self._secondary, 'ec2/removeMediaServer', data={'id': self.guids[self._primary]})
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)

        # wait if the secondary becomes new primary
        pass

        # check if the server time
        #TODO: continue (or remove, or rewrite) after the right logic will be determined

    ###################################################################

    def _prepare_inet_test(self, box, num):
        time.sleep(0)
        #out =
        self._call_box(box, '/vagrant/ctl.sh', 'timesync', 'prepare_isync', self._init_time[num])
        #sys.stdout.write("Box %s, output:\n%s\n" % (box, out))

    ###################################################################

    def TurnInetOn(self):
        #raw_input("[Press ENTER to continue with Internet time sync test init...]")
        self._prepare_test_phase(self._prepare_inet_test)
        self._check_time_sync()
        #TODO check for primary, make it primary if not bn
        # until that, set it to be sure
        self._setPrimaryServer(self._primary)
        self._check_time_sync(False)
        type(self)._secondary = self._get_secondary()
        log(LOGLEVEL.DEBUG + 9, "DEBUG: primary is %s, secondary is %s" % (self._primary, self._secondary))
        #raw_input("[Press ENTER to turn Inet up on secondary...]")
        #print self._call_box(self.hosts[self._secondary], '/vagrant/ctl.sh', 'timesync', 'iup')
        self._call_box(self.hosts[self._secondary], '/sbin/ifup', IF_EXT)
        time.sleep(INET_SYNC_TIMEOUT)
        itime_str = check_inet_time()
        self.assertTrue(itime_str, "Internet time request failed!")
        itime = struct.unpack('!I', itime_str)[0] - SHIFT_1900_1970
        idelta = itime - time.time() # get the difference between local ant inet time to use it later
        log(LOGLEVEL.DEBUG + 9, "DEBUG: time from internet: %s (%s); delta: %s" % (
            itime, time.asctime(time.localtime(itime)), idelta))
        #raw_input("[Press ENTER to continue...]")

        fail = False
        failed = []
        for boxnum in xrange(self.num_serv):
            btime = self._request_gettime(boxnum)
            itime = time.time() + idelta
            log(LOGLEVEL.INFO, "Server %s time %s; inet time %s" % (boxnum, btime['time'], itime))
            if abs(itime - btime['time']) > INET_GRACE:
                fail = True
                failed.append([boxnum])
                log(LOGLEVEL.ERROR, "Server at box %s hasn't sinchronized with Internet time in %s seconds. "
                    "Time delta %s, max delta allowed: %s, " %
                    (boxnum, INET_SYNC_TIMEOUT, abs(itime - btime['time']), INET_GRACE))

        self.assertFalse(fail, "Some of servers hasn't sinchronized with Internet time in %s seconds." % INET_SYNC_TIMEOUT)
        return
        #
        #self.assertAlmostEqual(itime, btime['time'], delta=INET_GRACE,
        #        msg="Server at box %s hasn't sinchronized with Internet time in %s seconds. "
        #            "Time delta %s, max delta allowed: %s, "%
        #            (boxnum, INET_SYNC_TIMEOUT, abs(itime - btime['time']), INET_GRACE))

    def ChangePrimarySystime(self):
        delta_before = self._serv_local_delta(self._primary)
        self.shift_box_time(self.hosts[self._primary], -12345)
        time.sleep(SYSTEM_TIME_SYNC_SLEEP * 10)
        delta_after = self._serv_local_delta(self._primary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
            msg="Primary server's time changed (%s) after changing of it's system time" %
                (delta_before - delta_after))
        self._check_time_sync(False)

    def KeepInetTimeAfterIfdown(self):
        """Check if the servers keep time, synchronized with the Internet, after the Internet connection goes down.
        """
        delta_before = self._serv_local_delta(self._primary)
        log(LOGLEVEL.INFO, "Turn off the internet connection at the box %s" % self._secondary)
        self._call_box(self.hosts[self._secondary], '/sbin/ifdown', IF_EXT)
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)
        delta_after = self._serv_local_delta(self._primary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="Primary server's time changed (%s) after the Internet connection "
                                   "on the secondary server was turned off" %
                                   (delta_before - delta_after))
        self._check_time_sync(False)

    def KeepInetTimeAfterSecondaryOff(self):
        """Check if the primary server keeps time after the secondary one (which was connected to the Internet) goes down.
        """
        delta_before = self._serv_local_delta(self._primary)
        self._mediaserver_ctl(self.hosts[self._secondary], 'stop')
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)
        delta_after = self._serv_local_delta(self._primary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="Primary server's time changed (%s) after the secondary server was turned off" %
                                   (delta_before - delta_after))

    def KeepInetTimeAfterSecondaryOn(self):
        """Check if the primary server keeps time after the secondary one starts up again. Also check symchronization.
        """
        delta_before = self._serv_local_delta(self._primary)
        self._mediaserver_ctl(self.hosts[self._secondary], 'start')
        self._wait_servers_up()
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)
        delta_after = self._serv_local_delta(self._primary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="Primary server's time changed (%s) after the secondary server was turned off" %
                                   (delta_before - delta_after))
        self._check_time_sync(False)

    def KeepInetTimeAfterRestartPrimary(self):
        """Restart primary and check time from the Internet is still kept.
        """
        delta_before = self._serv_local_delta(self._primary)
        self._mediaserver_ctl(self.hosts[self._primary], 'restart')
        self._wait_servers_up()
        time.sleep(SYSTEM_TIME_SYNC_SLEEP)
        delta_after = self._serv_local_delta(self._primary)
        self.assertAlmostEqual(delta_before, delta_after, delta=DELTA_GRACE,
                               msg="Primary server's time changed (%s) after it's restart" %
                                   (delta_before - delta_after))
        self._check_time_sync(False)

    @unittest.expectedFailure
    def BothRestart_SyncWithOS(self):
        """Restart both servers and check that now they are synchronized with the primary's OS time.
        """
        primary = self._primary # save it
        self._servers_th_ctl('stop')
        time.sleep(MINOR_SLEEP)
        self._servers_th_ctl('start')
        self._wait_servers_up()
        self._check_time_sync()
        self.assertEqual(primary, self._primary, "The primary server changed after both servers were restarted.")

    #@unittest.expectedFailure
    def PrimaryFollowsSystemTime(self):
        """Check if the primary server changes time when it's system time has been changed.
        """
        primary = self._primary # save it
        self.shift_box_time(self.hosts[self._primary], 5000)
        time.sleep(SYSTEM_TIME_SYNC_SLEEP * 4)
        self._check_time_sync()
        self.assertEqual(primary, self._primary, "The primary server changed after the previous primary's system time changed.")


class TimeSyncNoInetTest(TimeSyncTest):
    helpStr = "Time synchnonization without Internet access test"
    "TimeSyncTest with only SyncTimeNoInetTests suite"
    _suites = TimeSyncTest.filterSuites('SyncTimeNoInetTests')


class TimeSyncWithInetTest(TimeSyncTest):
    helpStr = "Time synchnonization with Internet access test"
    "TimeSyncTest with only InetTimeSyncTests suite"
    _suites = TimeSyncTest.filterSuites('InetTimeSyncTests')

