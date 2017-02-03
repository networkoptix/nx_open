# $Id$
# Artem V. Nikitin
# Virtual camera test

import time, os, json, uuid

from testbase import FuncTestCase
from pycommons.ComparisonMixin import ComparisonMixin
from pycommons.TestCamera import TestCameraMixin
from pycommons.Utils import secs2str
from pycommons.Logger import log, LOGLEVEL
from pycommons.HlsClient import HlsClient

WAIT_CHUNK = 2*60

class VirtualCameraTest(FuncTestCase, TestCameraMixin):
    "Virtual camera test"

    helpStr = 'Virtual camera test'
    _test_name = 'Virtual camera'
    _test_key = 'vcamera'

    # Test suites
    _suites = (
          ('VirtualCameraSuits', [
        'testCameraHistory'
          ]),
      )

    def setUp(self):
        FuncTestCase.setUp(self)
        TestCameraMixin.setUp(self)
        self.serverAddr1 = self.sl[0]
        self.serverAddr2 = self.sl[1]

    def tearDown(self):
        TestCameraMixin.tearDown(self)


    def __logSeq(self, items):
        log(LOGLEVEL.DEBUG, "Camera list:")
        for i, item in enumerate(items):
           ts = secs2str(int(item['timestampMs']) / 1000)
           ms = int(item['timestampMs']) % 1000
           srv = self.guids[0] in item['serverGuid'] and 'Server#1' or 'Server#2'
           srv+= " (%s)" % item['serverGuid']
           log(LOGLEVEL.DEBUG, "  #%d %s: %s.%d" % (i, srv, ts, ms))

    def __waitCameraHistory(self, servers, expGuids):
        log(LOGLEVEL.INFO, "Camera history (wait cycle) start...")
        responses = []
        for srv in servers:
            start = time.time()
            while True:
                try:
                    response = self.client.httpRequest(
                        srv,
                        "ec2/cameraHistory",
                        startTime = '0',
                        endTime='now',
                        cameraId=self.cameraGuid)
                    self.checkResponseError(response, "ec2/cameraHistory")
                    self.assertTrue(response.data, "Got unexpected empty response for 'ec2/cameraHistory'")
                    gotGuids = map(lambda x: x['serverGuid'], response.data[0]['items'])
                    self.__logSeq(response.data[0]['items'])
                    self.compare(expGuids, gotGuids, 'guids seq')
                    tsSeq = map(lambda x: x['timestampMs'], response.data[0]['items'])
                    self.assertIsSorted(tsSeq, 'timestampMs seq')
                    responses.append(response)
                    break
                except self.failureException:
                    remain = WAIT_CHUNK - (time.time() - start)
                    if remain <= 0:
                        raise
                    else:
                        time.sleep(5.0)
        self.compare(responses[0].data, responses[1].data)
        log(LOGLEVEL.INFO, "Camera history (wait cycle) done")

    def __waitCameraHistoryWithRestart(self, servers, expGuids):
        try:
            self.__waitCameraHistory(servers, expGuids)
            return
        except self.failureException:
            self.restartCamera()
        self.__waitCameraHistory(servers, expGuids)
        
    def testCameraHistory(self):
        "Switch camera between two servers and check history"
        log(LOGLEVEL.INFO, "1. Prepare test environment.")
        self._prepare_test_phase(self._stop_and_init)
        serverGuid1 = "{%s}" % self.guids[0]
        serverGuid2 = "{%s}" % self.guids[1]
        # Wait camera appearance
        self.waitCamera([self.serverAddr1, self.serverAddr2])
        
        log(LOGLEVEL.INFO, "2. Switch camera to server#1 and start recording.")
        self.switchCameraToServer(self.serverAddr1, serverGuid1)
        self.startCameraRecording(self.serverAddr1, serverGuid1)
        self.__waitCameraHistoryWithRestart(
            [self.serverAddr1, self.serverAddr2],
            [serverGuid1])

        log(LOGLEVEL.INFO, "3. Switch camera to server#2.")
        self.switchCameraToServer(self.serverAddr1, serverGuid2)
        self.__waitCameraHistoryWithRestart(
            [self.serverAddr1, self.serverAddr2],
            [serverGuid1, serverGuid2])
                
        log(LOGLEVEL.INFO, "4. Switch camera to server#1.")
        self.switchCameraToServer(self.serverAddr1, serverGuid1)
        self.__waitCameraHistoryWithRestart(
            [self.serverAddr1, self.serverAddr2],
            [serverGuid1, serverGuid2, serverGuid1])

        self.stopCameraRecording(self.serverAddr1, serverGuid1)

        # https://networkoptix.atlassian.net/browse/VMS-4180
        # HLS m3u list request with duration
        client = HlsClient(timeout = 30.0)
        response = client.requestM3U(
            self.serverAddr1,
            "{%s}" % self.guids[0], self.cameraData, duration = 3000)
        
        self.assertTrue(len(response.m3uList) > 0,
          "Unexpected M3U playlist size: %d" % len(response.m3uList))
        
