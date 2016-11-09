# -*- coding: utf-8 -*-
""" Database migration on version upgrade test.
Uses two servers, each of them starts with earlier version DB copy.
The DB copy contains some transactions with users, cameras, server settings and rules.
Checks if both servers start and merge their data correctly.
"""
__author__ = 'Danil Lavrentyuk'

import os, time
from functest_util import compareJson, textdiff
from stortest import StorageBasedTest

NUM_SERV=2
SERVERS_MERGE_WAIT=20
BACKUP_RESTORE_TIMEOUT=40
REALM_FIX_TIMEOUT=10
#BACKUP_DB_FILE="BackupRestoreTest.db.sqlite"
BACKUP_DB_FILE=""
DUMP_BEFORE="" # "data-before"
DUMP_AFTER="" # "data-after"

EXPECTED_REALM="networkoptix"

def _sleep(n):
    print "Sleep %s..." % n
    time.sleep(n)


def _clearDumps():
    for name in (DUMP_BEFORE, DUMP_AFTER, BACKUP_DB_FILE):
        try:
            if name is not None and name != '':
                os.remove(name)
        except Exception:
            pass


def _saveDump(name, data, mode = "t"):
    if name is not None and name != '':
        try:
            with open(name, "w"+mode) as f:
                print >>f, data
        except Exception as err:
            print "WARNING: failed to store FullInfo dump into file %s: %r" % (name, err)


class DBTest(StorageBasedTest):

    helpStr = "DB migration on version upgrade test"
    num_serv = NUM_SERV
    _test_name = "Database"
    _test_key = 'db'

    _suits = (
        ('DBTest', [
            'DBUpgradeTest',
            'BackupRestoreTest',
        ]),
    )

    _dbfiles = ['v2.4.1-box1.db', 'v2.4.1-box2.db']
    _ids = [
        '{62a54ada-e7c7-0d09-c41a-4ab5c1251db8}',
        '{88b807ab-0a0f-800e-e2c3-b640b31f3a1c}',
    ]

    _doPause = False

    @classmethod
    def tearDownClass(cls):
        cls._stopped.clear() # do not start at the end
        super(DBTest, cls).tearDownClass()

    @classmethod
    def _global_clear_extra_args(cls, num):
        return ()

    def _init_script_args(self, boxnum):
        print "DEBUG: box %s, set id %s" % (boxnum, self._ids[boxnum])
        return (self._dbfiles[boxnum], self._ids[boxnum])

    def _ensureRealm(self):
        print "Ensure the old realm used..."
        realmNotReady = set(xrange(self.num_serv))
        until = time.time() + REALM_FIX_TIMEOUT
        func = "api/moduleInformation" if self.before_3_0 else "api/getNonce?userName=admin"
        while realmNotReady:
            for server in realmNotReady.copy():
                data = self._server_request(server, func)
                #print "Response: %s" % (data,)
                try:
                    realm = data['reply']['realm']
                except Exception:
                    continue
                if realm == EXPECTED_REALM:
                    realmNotReady.discard(server)
            if realmNotReady:
                if time.time() >= until:
                    self.assertFalse(realmNotReady,
                                     "Servers %s didn't fixed HTTP digest realm in %s seconds"
                                      % (list(realmNotReady), REALM_FIX_TIMEOUT))
                #print "Time to fix %.1f" % (until - time.time())
                time.sleep(0.5)

    def DBUpgradeTest(self):
        """ Start both servers and check that their data are synchronized. """
        self._prepare_test_phase(self._stop_and_init)
        print "Wait %s seconds for server to upgrade DB and merge data..." % SERVERS_MERGE_WAIT
        time.sleep(SERVERS_MERGE_WAIT)
        self._ensureRealm()
        print "Now check the data"
        func = 'ec2/getFullInfo?extraFormatting'
        answers = [self._server_request(n, func, unparsed=True) for n in xrange(self.num_serv)]
        diff = compareJson(answers[0][0], answers[1][0])
        if diff.hasDiff():
            diffresult = textdiff(answers[0][1], answers[1][1], self.sl[0], self.sl[1])
            self.fail("Servers responses on %s are different: %s\nTextual diff results:\n%s" % (func, diff.errorInfo(), diffresult))

    def _waitOrInput(self, msg, sleep=0):
        if self._doPause:
            raw_input(msg+"...")
        elif sleep > 0:
            _sleep(sleep)

    def BackupRestoreTest(self):
        """ Check if backup/restore preserve all necessary data. """
        _clearDumps()
        WORK_HOST = 0  # which server do we check with backup/restore
        getInfoFunc = 'ec2/getFullInfo?extraFormatting'
        self._waitOrInput("Before dump")
        fulldataBefore = self._server_request(WORK_HOST, getInfoFunc, unparsed=True)
        _saveDump(DUMP_BEFORE, fulldataBefore[1])
        resp = self._server_request(WORK_HOST, 'ec2/dumpDatabase?format=json')
        #print "DEBUG: Returned data size: %s" % len(resp['data'])
        backup = resp['data']
        _saveDump(BACKUP_DB_FILE, backup, "b")
        _sleep(5)
        # Now change DB data -- add a camera
        self._add_test_camera(0, nodump=True)
        #
        self._waitOrInput("Before restore", 10)
        self._server_request(WORK_HOST, 'ec2/restoreDatabase', data={'data': backup}, nodump=True)
        save_guids = self.guids[:]
        self._waitOrInput("After restore", 10)
        self._wait_servers_up()
        self.assertSequenceEqual(save_guids, self.guids,
            "Server guids have changed after restore: %s -> %s" % (save_guids, self.guids))
        _sleep(5)
        start = time.time()
        stop = start + BACKUP_RESTORE_TIMEOUT
        cnt = 1
        while True:
            fulldataAfter = self._server_request(WORK_HOST, getInfoFunc, unparsed=True)
            diff = compareJson(fulldataBefore[0], fulldataAfter[0])
            if diff.hasDiff() and time.time() < stop:
                print "Try %d failed" % cnt
                cnt += 1
                time.sleep(1.5)
                continue
            break
        _saveDump(DUMP_AFTER, fulldataAfter[1])
        if diff.hasDiff():
            print "DEBUG: compareJson has found differences: %s" % (diff.errorInfo(),)
            diffresult = textdiff(fulldataBefore[1], fulldataAfter[1], "Before", "After")
            self.fail("Servers responses on %s are different:\n%s" % (getInfoFunc, diffresult))
        else:
            print "Success after %.1f seconds" % (time.time() - start,)
