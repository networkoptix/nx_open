# $Id$
# Artem V. Nikitin
# Test camera emulator

import os, time, json
from ServerEnvironment import ServerEnvironment
from ProcMgr import Process
from Config import config
from pycommons.Logger import LOGLEVEL, log
from pycommons.Rec import Rec
from MockClient import ClientMixin

WAIT_CAMERA_TIMEOUT=30
TEST_CAMERA_NAME="TestCameraLive"

class TestCameraProc(Process):

    CAMERA_BIN=os.path.join(ServerEnvironment.getBinPath(), "testcamera")
   
    def __init__(self):
        camera_env = os.environ.copy()
        camera_env['LD_LIBRARY_PATH']=ServerEnvironment.getLDLibPath()
        sample_file=os.path.join(config.get("General", "vagrantFolder"), "sample.mkv")
        command = [
            self.CAMERA_BIN, 'files="%s";count=1' % sample_file]
        Process.__init__(self, 'TestCamera', command, env = camera_env)


class TestCameraMixin(ClientMixin):
    
    # Test camera default attributes
    CAMERA_DEFAULT_ATTRIBUTES = {
         "audioEnabled":False,
         "backupType":"CameraBackupDefault",
         #"cameraId":"{e3e9a385-7fe0-3ba5-5482-a86cde7faf48}",
         "cameraName":TEST_CAMERA_NAME,
         "controlEnabled":True,
         "dewarpingParams":"{\"enabled\":false,\"fovRot\":0,\"hStretch\":1,\"radius\":0.5,\"viewMode\":\"1\",\"xCenter\":0.5,\"yCenter\":0.5}",
         "failoverPriority":"Medium",
         "licenseUsed":False,
         "maxArchiveDays":-30,
         "minArchiveDays":-1,
         "motionMask":"",
         "motionType":"MT_SoftwareGrid",
         #"preferredServerId":"{93d451dc-867e-8481-b983-c92aec8d9cfd}",
         "scheduleEnabled":False,
         "scheduleTasks":[
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":1,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"},
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":2,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"},
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":3,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"},
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":4,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"},
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":5,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"},
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":6,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"},
             {"afterThreshold":5,"beforeThreshold":5,"dayOfWeek":7,"endTime":86400,"fps":15,"recordAudio":False,"recordingType":"RT_Always","startTime":0,"streamQuality":"high"}],
         "secondaryStreamQuality":"SSQualityMedium",
         "userDefinedGroupName":""}

    def setUp(self):
        ClientMixin.setUp(self)
        self.camera = TestCameraProc()
        self.cameraGuid = None
        self.cameraData = None
        self.camera.start()

    def tearDown(self):
        ClientMixin.tearDown(self)
        self.camera.stop()

    def __checkCamera(self, response):
        if response.status == 200:
            for d in response.data:
                if d["name"] == TEST_CAMERA_NAME:
                    if not self.cameraGuid:
                        self.cameraGuid = d["id"]
                        self.cameraData = Rec(d)
                    return True
        return False

    def __cameraRecording(self, srv, srvGuid, scheduleEnabled):
        data = self.CAMERA_DEFAULT_ATTRIBUTES.copy()
        data["cameraId"] = self.cameraGuid
        data["scheduleEnabled"] = scheduleEnabled
        #data["scheduleTasks"] = []
        response = self.client.httpRequest(
            srv, "ec2/saveCameraUserAttributes",
            headers={'Content-Type': 'application/json'},
            data=json.dumps(data))
        self.checkResponseError(response, "ec2/saveCameraUserAttributes")

    def startCameraRecording(self, srv, srvGuid):
        self.__cameraRecording(srv, srvGuid, True)

    def stopCameraRecording(self, srv, srvGuid):
        self.__cameraRecording(srv, srvGuid, False)
        

    def waitCamera(self, servers):
        log(LOGLEVEL.INFO, "Camera appearance (wait cycle) start...")
        start = time.time()
        while True:
            checks = []
            for srv in servers:
                response = self.client.httpRequest(srv, "ec2/getCamerasEx")
                checks.append(self.__checkCamera(response))
            remain = WAIT_CAMERA_TIMEOUT - (time.time() - start)
            if filter(lambda x: not x, checks):
                if remain <= 0:
                    self.fail("Can't find test camera")
                else:
                    time.sleep(5.0)
            else:
                log(LOGLEVEL.INFO, "Camera appearance (wait cycle) done")
                return

    def restartCamera(self):
        self.camera.stop()
        time.sleep(5.0)
        self.camera.start()

    def  switchCameraToServer(self, srv, srvGuid):
        self.assertTrue(self.cameraGuid)
        data = {}
        data['id'] = self.cameraGuid
        data['parentId'] = srvGuid
        response = self.client.httpRequest(
            srv, "ec2/saveCamera",
            headers={'Content-Type': 'application/json'},
            data=json.dumps(data))
        self.assertEqual(200, response.status)
        response = self.client.httpRequest(
            srv, "ec2/getCamerasEx",
            id = self.cameraGuid)
        self.assertEqual(200, response.status)
        self.assertEqual(srvGuid, response.data[0]['parentId'])
