# -*- coding: utf-8 -*-
""" Legacy (written by earlier test-programmer) func.tests, based on unittest.TestCase
  and executed through unittest.main() call.
"""
__author__ = 'Danil Lavrentyuk'
import random
import sys, time
import unittest
import urllib2
import threading

from functest_util import *
from generator import *
from testbase import RunTests as RunBoxTests, LegacyTestWrapper, FuncTestMaster, getTestMaster

testMaster = getTestMaster()  # it's a singleton

class _pvt(object):
    "Used to hide LegacyFuncTestBase from default test loader."
    class LegacyFuncTestBase(unittest.TestCase):
        """Base class for test classes, called by unittest.main().
        Legacy from the first generation of functests.
        """
        _Lock = threading.Lock()  # Note: this lock is commin for all ancestor classes!
        failureException = LegacyTestFailure

        def _generateModifySeq(self):
            return None

        def _getMethodName(self):
            raise NotImplementedError()

        def _getObserverNames(self):
            raise NotImplementedError()

        def _defaultModifySeq(self, fakeData):
            # pick up a server randomly for each fakeData element
            return [(f, testMaster.getRandomServer()) for f in fakeData]

        def _defaultCreateSeq(self,fakeData):
            ret = []
            for f in fakeData:
                serverName = testMaster.getRandomServer()
                # add rollback cluster operations
                testMaster.unittestRollback.addOperations(self._getMethodName(), serverName, f[1])
                ret.append((f[0], serverName))
            return ret

        def _dumpFailedRequest(self, data, methodName):
            #FIXME rewrite it or remove!
            f = open("%s.failed.%.json" % (methodName,threading.active_count()),"w")
            f.write(data)
            f.close()

        def _sendRequest(self, methodName, d, server):
            url = "http://%s/ec2/%s" % (server, methodName)
            try:
                sendRequest(self._Lock, url, d, notify=True)
            except TestRequestError as err:
                print "ERROR in test %s: %s" % (self.__class__.__name__, err.message)
                self._dump_post_data(d)
                #self._dumpFailedRequest(d, methodName)
                self.fail("%s failed with %s" % (methodName, err.errMessage))

        def run(self, result=None):
            if result is None: result = self.defaultTestResult()
            result.startTest(self)
            testMethod = getattr(self, self._testMethodName)
            try:
                #FIXME Refactor error handling!
                try:
                    self.setUp()
                except Exception:
                    result.addError(self, sys.exc_info())
                    return

                ok = False
                try:
                    testMethod()
                    ok = True
                except self.failureException:
                    result.addFailure(self, sys.exc_info())
                    result.stop()
                except Exception:
                    result.addError(self, sys.exc_info())
                    result.stop()

                try:
                    self.tearDown()
                except Exception:
                    result.addError(self, sys.exc_info())
                    ok = False
                if ok: result.addSuccess(self)
            finally:
                result.stopTest(self)

        def _dump_post_data(self, data):
            pass

        def test(self):
            print "\n==================================="
            print "Test %s start!\n" % (self._getMethodName(),)

            postDataList = self._generateModifySeq()

            # skip this class
            if postDataList is None:
                return

            workerQueue = ClusterWorker(testMaster.threadNumber, len(postDataList))

            for test in postDataList:
                workerQueue.enqueue(self._sendRequest , (self._getMethodName(), test[0], test[1],))

            workerQueue.join()
            self.assertTrue(workerQueue.allOk(), workerQueue.getFailsMsg())

            time.sleep(testMaster.clusterTestSleepTime)
            for flag in (False, False, True):
                try:
                    testMaster.checkMethodStatusConsistent(self._getObserverNames())
                except ServerCompareFailure as err:
                    if flag:
                        print "DEBUG: %s. Try again." % str(err)
                        raise
                    time.sleep(1.5)
                else:
                    break

            print "Test %s done!" % (self._getMethodName())
            print "===================================\n"


class CameraTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = CameraDataGenerator()


    def _generateModifySeq(self):
        ret = []
        for _ in xrange(self._testCase):
            s = testMaster.getRandomServer()
            data = self._gen.generateCameraData(1,s)[0]
            testMaster.unittestRollback.addOperations(self._getMethodName(), s, data[1])
            ret.append((data[0],s))
        return ret

    def _getMethodName(self):
        return "saveCameras"

    def _getObserverNames(self):
        return ["getCameras"]


class UserTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = UserDataGenerator()

    def _generateModifySeq(self):
        return self._defaultCreateSeq(self._gen.generateUserData(self._testCase))

    def _getMethodName(self):
        return "saveUser"

    def _getObserverNames(self):
        return ["getUsers"]


""" temporary turned of because of special rights for /ec2/saveMediaServer request
class MediaServerTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = MediaServerGenerator()

    def _generateModifySeq(self):
        return self._defaultCreateSeq(self._gen.generateMediaServerData(self._testCase))

    def _getMethodName(self):
        return "saveMediaServer"

    def _getObserverNames(self):
        return ["getMediaServersEx"]
"""


class ResourceParamTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = ResourceDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateResourceParams(self._testCase))

    def _getMethodName(self):
        return "setResourceParams"

    def _getObserverNames(self):
        return ["getResourceParams"]


class ResourceRemoveTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = ResourceDataGenerator(self._testCase * 2)

    def _sendRequest(self, methodName, d, server):
        data = self._gen.generateRemoveResource(d)
        super(ResourceRemoveTest, self)._sendRequest(methodName, data, server)
        # here we'll get only on success
        testMaster.unittestRollback.takeOff(d)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateRemoveResourceIds(self._testCase))

    def _getMethodName(self):
        return "removeResource"

    def _getObserverNames(self):
        return ["getMediaServersEx", "getUsers", "getCameras"]

        #def _dump_post_data(self, data):
    #    print "ResourceRemoveTest data: %s" % (data,)
        #with open("%s-fails" % self._getMethodName(), "a") as f:
        #    f.write("%s\n" % data)



class CameraUserAttributeListTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = CameraUserAttributesListDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateCameraUserAttribute(self._testCase))

    def _getMethodName(self):
        return "saveCameraUserAttributesList"

    def _getObserverNames(self):
        return [testMaster.api.getCameraAttr.split('/')[1]]

    #def _dump_post_data(self, data):
        #print "CameraUserAttributeListTest data: %s" % (data,)
        #with open("%s-fails" % self._getMethodName(), "a") as f:
        #    f.write("%s\n" % data)


class ServerUserAttributesListDataTest(_pvt.LegacyFuncTestBase):
    _gen = None
    _testCase = testMaster.testCaseSize

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        self._testCase = testMaster.testCaseSize
        self._gen = ServerUserAttributesListDataGenerator(self._testCase * 2)

    def _generateModifySeq(self):
        return self._defaultModifySeq(self._gen.generateServerUserAttributesList(self._testCase))

    def _getMethodName(self):
        return testMaster.api.saveServerAttrList.split('/')[1]

    def _getObserverNames(self):
        return [testMaster.api.getServerAttr.split('/')[1]]

# The following test will issue the modify and remove on different servers to
# trigger confliction resolving.
class ResourceConflictionTest(_pvt.LegacyFuncTestBase):
    _testCase = testMaster.testCaseSize
    _conflictList = []

    def setTestCase(self,num):
        self._testCase = num

    def setUp(self):
        dataGen = ConflictionDataGenerator()

        print "Start confliction data preparation, this will generate Cameras/Users/MediaServers"
        dataGen.prepare(testMaster.testCaseSize)
        print "Confilication data generation done"

        self._testCase = testMaster.testCaseSize
        self._conflictList = [("removeResource","saveMediaServer",MediaServerConflictionDataGenerator(dataGen)),
            ("removeResource","saveUser",UserConflictionDataGenerator(dataGen)),
            ("removeResource","saveCameras",CameraConflictionDataGenerator(dataGen))]

    def _generateRandomServerPair(self):
        # generate first server here
        s1 = testMaster.getRandomServer()
        s2 = testMaster.getAnotherRandomServer(s1)
        return (s1, s2)

    def _generateResourceConfliction(self):
        return random.choice(self._conflictList)

    def _checkStatus(self):
        time.sleep(testMaster.clusterTestSleepTime)
        testMaster.checkMethodStatusConsistent(["getMediaServersEx", "getUsers", "getCameras"])

    # Overwrite the test function since the base method doesn't work here

    def test(self):
        workerQueue = ClusterWorker(testMaster.threadNumber, self._testCase * 2)

        print "===================================\n"
        print "Test:ResourceConfliction start!\n"

        for _ in xrange(self._testCase):
            conf = self._generateResourceConfliction()
            s1, s2 = self._generateRandomServerPair()
            data = conf[2].generateData()

            # modify the resource
            workerQueue.enqueue(self._sendRequest , (conf[1],data[0][0],s1,))
            # remove the resource
            workerQueue.enqueue(self._sendRequest , (conf[0],data[0][1],s2,))

        workerQueue.join()

        self._checkStatus()

        print "Test:ResourceConfliction finish!\n"
        print "===================================\n"


