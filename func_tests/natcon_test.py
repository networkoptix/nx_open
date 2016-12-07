# -*- coding: utf-8 -*-
"""
Connection behind NAT test
"""
__author__ = 'Danil Lavrentyuk'

import os, os.path, sys, time
#import subprocess
import unittest

import urllib2 # FIXME remove it

from functest_util import checkResultsEqual, generateKey
from testbase import *
from stortest import StorageBasedTest, TEST_CAMERA_ATTR, TEST_CAMERA_DATA, STORAGE_INIT_TIMEOUT
from rtsptests import SingleServerRtspPerf, RtspStreamTest, SingleServerHlsTest, HlsStreamingTest #, Camera
from pycommons.Logger import log, LOGLEVEL

NUM_NAT_SERV = 2  # 2 mediaservers are used: 1st before NAT, 2rd - behind NAT
                  # there is no mediaserver on the box with NAT
HOST_BEFORE_NAT = 0
HOST_BEHIND_NAT = 1

TEST_NATCAMERA_DATA = TEST_CAMERA_DATA.copy()
TEST_NATCAMERA_DATA['physicalId'] = TEST_NATCAMERA_DATA['mac'] = '11:22:33:33:22:11'
TEST_NATCAMERA_DATA['url'] = '192.168.109.62'

TEST_NATCAMERA_ATTR = TEST_CAMERA_ATTR.copy()
TEST_NATCAMERA_ATTR['cameraName'] = 'nat-camera'

class NatConnectionTestError(FuncTestError):
    pass


class NatSingleStreamingPerf(SingleServerRtspPerf):
    def _addCamera(self, c):
        #print "DEBUG: _addCamera server %s, guid %s, camera:\n%s" % (self._serverAddr, self._serverGUID, c)
        if c['parentId'] == self._serverGUID:  # this is the guid of the another server!
            super(SingleServerRtspPerf, self)._addCamera(c)


class NatRtspStreamTest(RtspStreamTest):
    _singleServerClass = NatSingleStreamingPerf


class NatSingleHlsPerf(SingleServerHlsTest):
    def _checkLineUrl(self, line):
        return line.startswith('/proxy/') and '/hls/' in line

    def _addCamera(self, c):
        #print "DEBUG: _addCamera server %s, guid %s, camera:\n%s" % (self._serverAddr, self._serverGUID, c)
        if c['parentId'] == self._serverGUID:  # this is the guid of the another server!
            super(SingleServerRtspPerf, self)._addCamera(c)


class NatHlsStreamingTest(HlsStreamingTest):
    _singleServerClass = NatSingleHlsPerf


class NatConnectionTest(StorageBasedTest):  # (FuncTestCase):

    helpStr = "Connection behind NAT test"
    _test_name = "NAT Connection"
    _test_key = "natcon"
    _suites = (
        ('NatConnectionTests', [
            'VMPreparation',
            'TestDataSynchronization',
            'TestHTTPForwarding',
            'TestRtspHttp',
         ]),
    )
    num_serv = NUM_NAT_SERV # the 1st server should be "before" NAT, the 2nd - behind NAT
    _fill_storage_script = 'fill_stor.py'
    _prepared = False


    ################################################################

    def _init_cameras(self):
        self._add_test_camera(HOST_BEFORE_NAT, log_response=0)
        self._add_test_camera(HOST_BEHIND_NAT,
            camera=self.new_test_camera(HOST_BEHIND_NAT, TEST_NATCAMERA_DATA),
            cameraAttr=TEST_NATCAMERA_ATTR, log_response=0)

    @classmethod
    def isFailFast(cls, suit_name=""):
        return False

    def _prepareKeys(self, srv_index):
        passwd = self.config.get("General","password")
        user = self.config.get("General","username")
        answer = self._server_request(srv_index, "api/getNonce")
        if answer is not None and answer.get("error", '') not in ['', '0', 0]:
            self.fail("api/getNonce request returned API error %s: %s" % \
                      (answer["error"], answer.get("errorString","")))
        nonce = answer["reply"]["nonce"]
        realm = answer["reply"]["realm"]
        getKey =  generateKey('GET', user, passwd, nonce, realm)
        postKey =  generateKey('POST', user, passwd, nonce, realm)
        return getKey, postKey

    def VMPreparation(self):
        "Join servers into one system"
        log(LOGLEVEL.INFO, "Server list: %s" % self.sl)
        self._prepare_test_phase(self._stop_and_init)
        getKey, postKey = self._prepareKeys(HOST_BEHIND_NAT)
        func = ("api/mergeSystems?url=http://%s&"
               "getKey=%s&postKey=%s" %
                (self.sl[0], getKey, postKey))
        answer = self._server_request(HOST_BEHIND_NAT, func)
        #print "Answer: %s" % (answer,)
        if answer is not None and answer.get("error", '') not in ['', '0', 0]:
            self.fail("mergeSystems request returned API error %s: %s\nRequest was: %s" % (answer["error"], answer.get("errorString",""), func))
        #print "mergeSystems sent, waiting"
        time.sleep(1)
        # get server's IDs
        self._wait_servers_up()
        self._prepared = True

    _sync_test_requests = ["getResourceParams", "getMediaServersEx", "getCamerasEx", "getUsers"]

    def TestDataSynchronization(self):
        if self._prepared:
            self.skipTest("Test suite preparatoin failed")
        for method in self._sync_test_requests:
            responseList = []
            for server in self.sl:
                log(LOGLEVEL.DEBUG + 9, "Request http://%s/ec2/%s" % (server, method))
                responseList.append((urllib2.urlopen("http://%s/ec2/%s" % (server, method)),server))
            # checking the last response validation
            checkResultsEqual(responseList, method)

    _func_to_forward = 'api/moduleInformation'

    def TestHTTPForwarding(self):
        if self._prepared:
            self.skipTest("Test suite preparatoin failed")
        "Uses request, returning different data for diаferent servers. Checks direct and proxied requests."
        direct_answer1 = self._server_request(1, self._func_to_forward)
        fwd_answer1 = self._server_request(0, self._func_to_forward, headers={'X-server-guid': self.guids[1]})
        self.assertEqual(direct_answer1, fwd_answer1, "Request inner server through outer returns different result then direct request")
        direct_answer2 = self._server_request(0, self._func_to_forward)
        fwd_answer2 = self._server_request(1, self._func_to_forward, headers={'X-server-guid': self.guids[0]})
        self.assertEqual(direct_answer2, fwd_answer2, "Request outer server through inner returns different result then direct request")
        self.assertNotEqual(direct_answer1, direct_answer2, "Both servers return the same moduleInformation data")

    def TestRtspHttp(self):
        if self._prepared:
            self.skipTest("Test suite preparatoin failed")
        self._load_storage_info(STORAGE_INIT_TIMEOUT)
        self._init_cameras()
        guids = self.guids[0:2]
        guids.reverse()  # to ask another server's data from each server
        self.config.rtset("ServerUUIDList", guids) # [quote_guid(guid) for guid in self.guids]
        self._fill_storage('streaming', HOST_BEFORE_NAT, "streaming")
        self._fill_storage('streaming', HOST_BEHIND_NAT, "streaming")
        self.assertTrue(NatRtspStreamTest(self.config).run(),
                "Multi-proto streaming test failed")
        self.assertTrue(NatHlsStreamingTest(self.config).run(),
                "HLS streaming test failed")

