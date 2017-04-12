# -*- coding: utf-8 -*-
"""
HLS, WEBM and RTSP sreaming test (to use in the autotest sequence)
"""
__author__ = 'Danil Lavrentyuk'
import time
#import unittest

from rtsptests import RtspStreamTest, HlsStreamingTest
from stortest import StorageBasedTest, checkInit  #, TEST_CAMERA_DATA
from functest_util import quote_guid
from testbase import NUM_SERV
from pycommons.Logger import log, LOGLEVEL

_NUM_STREAM_SERV = 1
_WORK_HOST = 0

class StreamingTest(StorageBasedTest):
    helpStr = "Streaming test"
    _test_name = "Streaming"
    _test_key = "stream"
    _suites = (
        ('StreamingTests', [
            'Initialization',
            'MultiProtoStreamingTest',
            'HlsStreamingTest'
         ]),
    )
    num_serv = _NUM_STREAM_SERV
    _fill_storage_script = 'fill_stor.py'

    @classmethod
    def isFailFast(cls, suit_name=""):
        return False

    @classmethod
    def globalInit(cls, config):
        super(StreamingTest, cls).globalInit(config)
        cls._duplicateConfig()
        cls.config.rtset("ServerList", [cls.sl[_WORK_HOST]])

    @classmethod
    def setUpClass(cls):
        super(StreamingTest, cls).setUpClass()
        cls._initFailed = False

    def _init_cameras(self):
        self._add_test_camera(_WORK_HOST, log_response=True)

    def _other_inits(self):
        "Prepare a single camera data"
        time.sleep(1)
        self.config.rtset("ServerUUIDList", self.guids[:]) # [quote_guid(guid) for guid in self.guids]
        answer = self._server_request(_WORK_HOST, 'ec2/getCamerasEx')
        log(LOGLEVEL.INFO, "_other_inits: before _fill_storage getCamerasEx(%s) response: '%s'" % (_WORK_HOST, answer))
        #raw_input("Before filling storage.... Press ENTER")
        self._fill_storage('streaming', _WORK_HOST, "streaming")
        log(LOGLEVEL.DEBUG + 9, "DEBUG0: _fill_storage finished at %s" % int(time.time()))
        answer = self._server_request(_WORK_HOST, 'ec2/getCamerasEx')
        log(LOGLEVEL.INFO, "_other_inits: after _fill_storage getCamerasEx(%s) response: '%s'" % (_WORK_HOST, answer))

    @checkInit
    def MultiProtoStreamingTest(self):
        "Check RTSP and HTTP streaming from server"
        self.assertTrue(RtspStreamTest(self.config).run(),
                        "Multi-proto streaming test failed")

    @checkInit
    def HlsStreamingTest(self):
        "Check HLS streaming from server"
        self.assertTrue(HlsStreamingTest(self.config).run(),
                        "HLS streaming test failed")


class HlsOnlyTest(StreamingTest):
    helpStr = "HLS streaming test only"
    _test_name = "HLS-only streaming"
    _test_key = "hlso"
    _suites = (
        ('HlsOnlyTest', [
            'Initialization',
            'HlsStreamingTest'
         ]),
    )
