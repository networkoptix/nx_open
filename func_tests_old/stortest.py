# -*- coding: utf-8 -*-
"""
Storages functional tests:
1. Backup storage test
2. Multiarchive storage test
Both imported and called by functest.py
Their common base class StorageBasedTest also used in other tests.
"""
__author__ = 'Danil Lavrentyuk'
import os, os.path, copy, sys, time
import hashlib, urllib
#import urllib2
import subprocess
#import traceback
import uuid
from pipes import quote as shquote
from functools import wraps
from pycommons.Logger import log, LOGLEVEL
#import pprint

from functest_util import ClusterLongWorker, get_server_guid, unquote_guid, CAMERA_ATTR_EMPTY
from testbase import *

mypath = os.path.dirname(os.path.abspath(sys.argv[0]))
multiserv_interfals_fname = os.path.join(mypath, "multiserv_intervals.py")


BACKUP_STORAGE_READY_TIMEOUT = 60  # seconds
BACKUP_START_TIMEOUT = 20  # seconds
BACKUP_MAX_DURATION = 60  #seconds
STORAGE_INIT_TIMEOUT = 10

_NUM_SERV_BAK = 1
_NUM_SERV_MSARCH = 2
_WORK_HOST = 0

TEST_CAMERA_TYPE_ID = "f9c03047-72f1-4c04-a929-8538343b6642"

_CameraMAC = '11:22:33:44:55:66'

TEST_CAMERA_DATA = {
    'id': '',
    'parentId': '',# put the server guid here
    'mac': _CameraMAC,
    'physicalId': _CameraMAC,
    'manuallyAdded': False,
    'model': 'test-camera',
    'groupId': '',
    'groupName': '',
    'statusFlags': '',
    'vendor': 'test-v',
    'name': 'test-camera',
    'url': '192.168.109.63',
    'typeId': TEST_CAMERA_TYPE_ID
}

def _mkTestCameraAttr(emptyCameraAttr):
    data = emptyCameraAttr.copy()
    data.update({
        'scheduleEnabled': False,
    #    'backupType': "CameraBackupBoth",
        'backupType': 3,
        'cameraName': 'test-camera',
    })
    return data

TEST_CAMERA_ATTR = _mkTestCameraAttr(CAMERA_ATTR_EMPTY)

_serverIdField = 'serverId'
SERVER_USER_ATTR = {
    _serverIdField: '', # put the server guid here
    'maxCameras': 10,
    'isRedundancyEnabled': False,
    'serverName': '',
    'backupType': 'BackupSchedule',
    'backupDaysOfTheWeek': 0x7f,
    'backupStart': 0, # == 00:00:00
    'backupDuration': -1,
    'backupBitrate': -1,
}

TMP_STORAGE = '/tmp/bstorage'  # directory path on the server side to create backup storage in


class BackupStorageTestError(FuncTestError):
    pass


def _mk_id(uniqId):
    h = hashlib.md5(uniqId).hexdigest()
    return '-'.join((h[:8],h[8:12],h[12:16],h[16:20],h[20:]))


def checkInit(meth):
    @wraps(meth)
    def w(self, *args, **kwargs):
        if self._initFailed:
            self.skipTest("not initialized")
        return meth(self, *args, **kwargs)
    return w


def _parseStoreages(response):
    return [s for s in response if s['storageType'] == 'local']


class StorageBasedTest(FuncTestCase):
    """ Some common logic for storage tests.
    """
    num_serv = 0
    _storages = dict()
    _fill_storage_script = ''
    test_camera_id = None
    test_camera_physical_id = None
    _initFailed = False

    @classmethod
    def isFailFast(cls, suit_name=""):
        # it could depend on the specific suit
        return False

    @classmethod
    def globalInit(cls, config):
        super(StorageBasedTest, cls).globalInit(config)
        cls.test_camera_id = [0 for _ in xrange(cls.num_serv)]
        cls.test_camera_physical_id = cls.test_camera_id[:]

    @classmethod
    def _configureVersion(cls, versionStr):
        super(StorageBasedTest, cls)._configureVersion(versionStr)
        global _serverIdField
        if _serverIdField != cls.api.serverId:
            SERVER_USER_ATTR[cls.api.serverId] = SERVER_USER_ATTR.pop(_serverIdField)
            _serverIdField = cls.api.serverId
        TEST_CAMERA_ATTR.clear()
        TEST_CAMERA_ATTR.update(_mkTestCameraAttr(CAMERA_ATTR_EMPTY))

    def _load_storage_info(self, timeout=0):
        "Get servers' storage space data"
        until = time.time() + timeout if timeout > 0 else 0
        for num in xrange(self.num_serv):
            st = None
            while not st:
                st = _parseStoreages(self._server_request(num, 'ec2/getStorages?id=%s' % self.guids[num]))
                if not st and until and time.time() > until:
                    raise AssertionError(
                        "No storages reroprted from server %s during %s seconds" % (num, timeout))
            self._storages[num] = st
            #print "[DEBUG] Storages found:"
            #for s in self._storages[num]:
            #    print s
                #print "%s: %s, storageType %s, isBackup %s" % (s['storageId'], s['url'], s['storageType'], s['isBackup'])

    @classmethod
    def _duplicateConfig(cls):
        cls.config = copy.copy(cls.config)
        cls.config.runtime = cls.config.runtime.copy()

    @classmethod
    def new_test_camera(cls, boxnum, camData=TEST_CAMERA_DATA):
        "Creates initial dict of camera data."
        data = camData.copy()
        data['id'] = _mk_id(data['physicalId'])
        cls.test_camera_id[boxnum] = data['id']
        cls.test_camera_physical_id[boxnum] = data['physicalId']
        return data

#    @classmethod
#    def _duplicateConfig(cls):
#        cls.config = copy.copy(cls.config)
#        cls.config.runtime = cls.config.runtime.copy()

    def _add_test_camera(self, boxnum, camera=None, log_response=False, cameraAttr=TEST_CAMERA_ATTR, nodump=False):
        camera = self.new_test_camera(boxnum) if camera is None else camera.copy()
        camera['parentId'] = self.guids[boxnum]
        self._server_request(boxnum, 'ec2/saveCamera', camera, nodump=nodump)
        answer = self._server_request(boxnum, 'ec2/getCameras')
        if log_response:
            log(LOGLEVEL.INFO,  "_add_test_camera: getCameras(%s) response: '%s'" % (boxnum, answer))
        for c in answer:
            if c['parentId'] == self.guids[boxnum]:
                self.assertEquals(unquote_guid(c['id']), self.test_camera_id[boxnum], "Failed to assign a test camera to to a server")
        attr_data = [cameraAttr.copy()]
        attr_data[0][self.api.cameraId] = self.test_camera_id[boxnum]
        self._server_request(boxnum, 'ec2/saveCameraUserAttributesList', attr_data, nodump=nodump) # return None
        answer = self._server_request(boxnum, 'ec2/getCamerasEx')
        if log_response:
            log(LOGLEVEL.INFO, "_add_test_camera: getCamerasEx(%s) response: '%s'" % (boxnum, answer))


    def _fill_storage(self, mode, boxnum, arg):
        log(LOGLEVEL.INFO, "Server %s: Filling the main storage with the test data." % boxnum)
        self._call_box(self.hosts[boxnum], "python", shquote("/vagrant/" + self._fill_storage_script), mode,
                       shquote(self._storages[boxnum][0]['url']), self.test_camera_physical_id[boxnum], arg)
        time.sleep(0.5)
        answer = self._server_request(boxnum, 'api/rebuildArchive?action=start&mainPool=1')
        #print "rebuildArchive start: %s" %(answer,)
        try:
            state = answer["reply"]["state"]
        except Exception:
            state = ''
        while state != 'RebuildState_None':
            time.sleep(0.8)
            answer = self._server_request(boxnum, 'api/rebuildArchive?mainPool=1')
            log(LOGLEVEL.DEBUG + 9, "rebuildArchive: %s" %(answer,))
            try:
                state = answer["reply"]["state"]
            except Exception:
                pass

    @classmethod
    def _need_clear_box(cls, num):
        return super(StorageBasedTest, cls)._need_clear_box(num) and num in cls._storages.iterkeys()

    @classmethod
    def _global_clear_extra_args(cls, num):
        return (cls._storages[num][0]['url'], TMP_STORAGE)

    def _init_cameras(self):
        pass

    def Initialization(self):
        """
        Common initial preparatioin code for subclasses
        Re-initialize with clear db, prepare a single camera data.
        """
        try:
            self._prepare_test_phase(self._stop_and_init)
            time.sleep(0.5)
            self._load_storage_info(STORAGE_INIT_TIMEOUT)
            self._init_cameras()
            time.sleep(0.5)
            self._other_inits()
            time.sleep(0.1)
        except Exception:
            type(self)._initFailed = True
            raise

    def _other_inits(self):
        "Specific initialization code for subclasses to be call under InitialRestartAndPrepare's try"
        pass

    def _checkInit(self):
        if self._initFailed:
            self.skipTest("not initialized")


    #def _has_camera(self, host, cameraName):
    #    "Checks if the host has already got a camera with such name"
    #    answer = self._server_request(host, 'ec2/getCameras')
    #    for c in answer:
    #        #print "Found camera '%s' at server %s" % (c['name'], c['parentId'])
    #        if c['name'] == cameraName and c['parentId'] == self.guids[host]:
    #            self.test_camera_id = c['id']
    #            self.test_camera_physical_id = c['physicalId']  # TODO change for a list of ids
    #            return True
    #    return False



class BackupStorageTest(StorageBasedTest):
    """
    The test for backup storage functionality.
    Creates a backup storage and tries to start the backup procedure both manually and scheduled.
    """
    helpStr = "Backup storage test"
    _test_name = "Backup Storage"
    _test_key = 'bstorage'
    num_serv = _NUM_SERV_BAK
    _fill_storage_script = 'fill_stor.py'

    _suites = (
        ('BackupTests', [
            'Initialization',
            'ScheduledBackupTest',
            'BackupByRequestTest',
        ]),
    )

    ################################################################

    def _init_cameras(self):
        self._add_test_camera(_WORK_HOST)

    def _create_backup_storage(self, boxnum):
        self._call_box(self.hosts[boxnum], "rm", '-rf', TMP_STORAGE)
        self._call_box(self.hosts[boxnum], "mkdir", '-p', TMP_STORAGE)
        data = self._server_request(boxnum, 'ec2/getStorages?id=' + self.guids[boxnum])
        self.assertIsNotNone(data, 'ec2/getStorages returned empty data')
        #print "DEBUG: Storages: "
        #pprint.pprint(data)
        data = data[0]
        data['id'] = new_id = str(uuid.uuid4())
        data['isBackup'] = True
        data['url'] = TMP_STORAGE
        self._server_request(boxnum, 'ec2/saveStorage', data=data)
        data = self._server_request(boxnum, 'ec2/getStorages?id=' + self.guids[boxnum])
        #print "DEBUG: New storage list:"
        #pprint.pprint(data)
        t = time.time() + BACKUP_STORAGE_READY_TIMEOUT
        new_id = '{' + new_id + '}'
        while True:
            time.sleep(1)
            if time.time() > t:
                self.fail("Can't initialize the backup storage. It doesn't become ready. (Timed out)")
            data = self._server_request(boxnum, 'ec2/getStatusList?id=' + new_id)
            try:
                #print "DEBUG: data = %s" % (data,)
                if data and data[0] and data[0]["id"] == new_id:
                    if data[0]["status"] == "Online":
                        log(LOGLEVEL.INFO, "Backup storage is ready for backup.")
                        return
                    #else:
                    #    print "Backup storage status: %s" % data[0]["status"]
            except Exception:
                pass

    def _wait_backup_start(self):
        end = time.time() + BACKUP_START_TIMEOUT
        while time.time() < end:
            data = self._server_request(_WORK_HOST, 'api/backupControl/')
            log(LOGLEVEL.DEBUG + 9, "DEBUG0: backupControl reply: %s" % (data,))
            if data['reply']['state'] != "BackupState_None":
                return
            time.sleep(1)
        self.fail("Backup didn't started for %s seconds (or was ended before the first check)" % BACKUP_START_TIMEOUT)

    def _wait_backup_end(self):
        end = time.time() + BACKUP_MAX_DURATION
        while time.time() < end:
            time.sleep(1)
            data = self._server_request(_WORK_HOST, 'api/backupControl/')
            log(LOGLEVEL.DEBUG + 9, "DEBUG0: backupControl reply: %s" % (data,))
            try:
                if data['reply']['state'] == "BackupState_None":
                    return
            except Exception:
                pass
        self.fail("Backup goes too log, more then %s seconds" % BACKUP_MAX_DURATION)

    def _check_backup_result(self):
        try:
            boxssh(self.hosts[_WORK_HOST], ('/vagrant/diffbak.sh', self._storages[_WORK_HOST][0]['url'], TMP_STORAGE))
        except subprocess.CalledProcessError as err:
            DIFF_FAIL_CODE = 1
            DIFF_FAIL_MSG = "DIFFERENT"
            if err.returncode == DIFF_FAIL_CODE:
                if err.output.strip() == DIFF_FAIL_MSG:
                    self.fail("The main storage and the backup storage contents are different")
                else:
                    log(LOGLEVEL.ERROR, "ERROR: Backup content check returned code %s. It's output:\n%s" % (err.returncode, err.output))
                    self.fail("The main storage and the backup storage contents are possibly different")
            else:
                log(LOGLEVEL.ERROR, "ERROR: Backup content check returned code %s. It's output:\n%s" % (err.returncode, err.output))
                self.fail("Backup content check failed with code %s and output:\n%s" % (err.returncode, err.output))

    ################################################################

    def _other_inits(self):
        self._create_backup_storage(_WORK_HOST)

    @checkInit
    def ScheduledBackupTest(self):
        "In fact it tests that scheduling backup for a some moment before the current initiates backup immidiately."
        #raw_input("[Press ENTER to start ScheduledBackupTest]")
        data = SERVER_USER_ATTR.copy()
        data[_serverIdField] = self.guids[_WORK_HOST]
        data['backupType'] = 'BackupManual'
        self._server_request(_WORK_HOST, self.api.saveServerAttrList, data=[data])
        time.sleep(0.1)
        #print "DEBUG0: StorageSpace: %s" % (self._server_request(_WORK_HOST, 'api/storageSpace'))
        self._call_box(self.hosts[_WORK_HOST], "/vagrant/ctl.sh", "bstorage", "rmstorage",
                       shquote(self._storages[_WORK_HOST][0]['url']))
        self._fill_storage('random', _WORK_HOST, "step1")
        data['backupType'] = 'BackupSchedule'
        time.sleep(0.1)
        #print "DEBUG: %s: %s" % (self.api.saveServerAttrList, data,)
        self._server_request(_WORK_HOST, self.api.saveServerAttrList, data=[data])
        self._wait_backup_start()
        self._wait_backup_end()
        self._check_backup_result()

    @checkInit
    def BackupByRequestTest(self):
        #raw_input("[Press ENTER to start BackupByRequestTest]")
        data = SERVER_USER_ATTR.copy()
        data[_serverIdField] = self.guids[_WORK_HOST]
        data['backupType'] = 'BackupManual'
        self._server_request(_WORK_HOST, self.api.saveServerAttrList, data=[data])
        time.sleep(0.1)
        self._fill_storage('random', _WORK_HOST, "step2")
        time.sleep(1)
        #print "DEBUG0: StorageSpace: %s" % (self._server_request(_WORK_HOST, 'api/storageSpace'))
        data = self._server_request(_WORK_HOST, 'api/backupControl/?action=start')
        log(LOGLEVEL.INFO, "backupControl start: %s" % data)
        self._wait_backup_end()
        #raw_input("Press ENTER to continue...")
        time.sleep(0.1)
        self._check_backup_result()


class MultiserverArchiveTest(StorageBasedTest):
    helpStr = "Multiserver archive test"
    _test_name = "Multiserver Archive"
    _test_key = "msarch"
    num_serv = _NUM_SERV_MSARCH
    _fill_storage_script = 'fill_stor.py'
    time_periods_single = []
    time_periods_joined = []

    _suites = (
        ('MultiserverArchiveTest', [
            'Initialization',
            'CheckArchiveMultiserv',
            'CheckArchivesSeparated',
        ]),
    )

    ################################################################

    def _init_cameras(self):
        c = self.new_test_camera(0)
        for num in xrange(_NUM_SERV_MSARCH):
            if num > 0:
                self.test_camera_id[num] = self.test_camera_id[0]
                self.test_camera_physical_id[num] = self.test_camera_physical_id[0]
            self._worker.enqueue(self._add_test_camera, (num, c, False))
        self._worker.joinQueue()

    def _getRecordedTime(self, boxnum, flat=True):
        req = 'ec2/recordedTimePeriods?physicalId=%s' % self.test_camera_physical_id[boxnum]
        if flat:
            req += '&flat' # ! it's not bool parameter, it's existance is it's value
        return self._server_request(boxnum, req)

    def _compare_chunks(self, boxnum, server, prepared):
        fail_count = 0
        i = 0
        serv_str = "" if boxnum is None else (' (%d)' % boxnum)
        for chunk in server:
            if i >= len(prepared):
                log(LOGLEVEL.ERROR, "FAIL: Extra data in server%s answer at position %s: %s" % (serv_str, i, chunk))
                fail_count += 1
            elif int(chunk['startTimeMs']) != prepared[i]['start'] + self.base_time:
                log(LOGLEVEL.ERROR,"FAIL: Chunk %s start time differs: server%s %s, prepared %s" % (
                    i, serv_str, int(chunk['startTimeMs']), prepared[i]['start'] + self.base_time))
                fail_count += 1
            elif int(chunk['durationMs']) != prepared[i]['duration']:
                log(LOGLEVEL.ERROR, "FAIL: Chunk %s duration differs: server%s %s, prepared %s" % (
                    i, serv_str, chunk['durationMs'], prepared[i]['duration']))
                fail_count += 1
            if fail_count >= 10:
                break
            i += 1
        if fail_count < 10 and i < len(prepared):
            log(LOGLEVEL.ERROR, "FAIL: Server%s anser shorter then prepared data (%s vs %s)" % (serv_str, i, len(prepared)))
        self.assertEqual(fail_count, 0, "Server%s reports chunk list different from the prepared one." % serv_str)

    ################################################################

    def _other_inits(self):
        log(LOGLEVEL.INFO, "Filling archives with data...")
        for num in xrange(_NUM_SERV_MSARCH):
            self._fill_storage('multiserv', num, str(num))
        log(LOGLEVEL.INFO, "Done. Wait a bit...")
        tmp = {}
        execfile(multiserv_interfals_fname, tmp)
        type(self).time_periods_single = tmp['time_periods_single']
        type(self).time_periods_joined = tmp['time_periods_joined']
        time.sleep(20)

    
    def _changeSystemId(self, host, new_guid):
        res = self._server_request(host, "api/configure?%s" % \
                                   urllib.urlencode({"localSystemId": new_guid}))
        self.assertEqual(res['error'], "0",
            "api/configure failed to set a new localSystemId %s for the server %s: %s" % (new_guid, host, res['errorString']))
        

    @checkInit
    def CheckArchiveMultiserv(self):
        "Checks recorded time periods for both servers joined into one system."
        answer = [self._getRecordedTime(num) for num in xrange(self.num_serv)]
        for a in answer:
            self.assertTrue(a["error"] == '0' and a["errorString"] == '',
                "ec2/recordedTimePeriods request to the box %s returns error %s: %s" % (num, a["error"], a["errorString"]))
        self.assertEqual(answer[0]["reply"], answer[1]["reply"],
                "Boxes return different answers to the ec2/recordedTimePeriods request")
        #for chunk in answer[0]['reply'][:10]:
        #    print chunk
        type(self).base_time = int(answer[0]['reply'][0]['startTimeMs'])
        #print "DEBUG: base = %s" % self.base_time
        self._compare_chunks(None, answer[0]['reply'], self.time_periods_joined)

    def _splitSystems(self):
        log(LOGLEVEL.INFO, "Split servers into two systems")
        guid =  '{%s}' % uuid.uuid4()
        self._changeSystemId(1, guid)
        time.sleep(1)
        info0 = self._server_request(0, 'ec2/testConnection')
        info1 = self._server_request(1, 'ec2/testConnection')
        self.assertEqual(info1['localSystemId'], guid, "Failed to give server 1 new system id")
        self.assertNotEqual(info0['localSystemId'], guid, "Server 0 system id also has changed")

    @checkInit
    def CheckArchivesSeparated(self):
        self._splitSystems()
        answer = [[], []]
        uri = ['ec2/recordedTimePeriods?physicalId=%s&flat' % self.test_camera_physical_id[n] for n in xrange(self.num_serv)]
        for atempt_count in xrange(10):
            answer[0] = self._server_request(0, uri[0])['reply'][:]
            time.sleep(0.2)
            answer[1] = self._server_request(1, uri[1])['reply'][:]
            #print "0: %s, %s..." % (id(answer[0]), answer[0][:10])
            #print "1: %s, %s..." % (id(answer[1]), answer[1][:10])
            if answer[0] != answer[1]:
                break
            log(LOGLEVEL.ERROR, "Attempt %s failed!" % atempt_count)
            time.sleep(1)
        for i in xrange(min(len(answer[0]), len(answer[1]))):
            if answer[0][i] == answer[1][i]:
                log(LOGLEVEL.INFO, "Separated servers report the same chunk: %s, %s" % (answer[0][i], answer[1][i]))
        self.assertFalse(answer[0] == answer[1], "Separated servers report the same chunk list")
        #print "Server 0\n" + "\n".join(str(chunk) for chunk in answer[0][:4])
        self._compare_chunks(0, answer[0], self.time_periods_single[0])
        #print "Server 1\n" + "\n".join(str(chunk) for chunk in answer[1][:4])
        self._compare_chunks(1, answer[1], self.time_periods_single[1])
        #print "Join servers back"
        #self._change_system_name(1, sysname)  # it's done by _clear_storage_script 'ms_clear.sh'
        time.sleep(0.1)
