# $Id$
# Artem V. Nikitin
# Merge test (https://networkoptix.atlassian.net/browse/TEST-177)

import time, os, json, uuid

from testbase import FuncTestCase
from pycommons.ComparisonMixin import ComparisonMixin
from pycommons.MockClient import ClientMixin, DigestAuthClient as Client
from pycommons.Utils import bool2str, str2bool
from pycommons.FuncTest import execVBoxCmd, MEDIA_SERVER_DIR, tlog
from pycommons.Logger import LOGLEVEL
from pycommons.Config import config
from functest_util import generateKey

TEST_SYSTEM_NAME_1="MergeTestSystem1"
TEST_SYSTEM_NAME_2="MergeTestSystem2"
CLOUD_SERVER='cloud-dev.hdw.mx'
CLOUD_USER_NAME='anikitin@networkoptix.com'
CLOUD_USER_PWD='qweasd123'
DEFAULT_CUSTOMIZATION='default'
SETUP_CLOUD_TIMEOUT=60

SYSTEM_SETTINGS_1 = {
  "systemSettings":
  {
    "cameraSettingsOptimization":"false",
    "autoDiscoveryEnabled":"false",
    "statisticsAllowed": "false"
   }
}

def getAdminGuid(response):
    for d in response.data:
        if d['name'] == 'admin':
            return d['id']
    return None
        
class MergeSystemTest(FuncTestCase, ClientMixin):
    "Merge systems test"

    helpStr = 'Merge system test'
    _test_name = 'Merge system'
    _test_key = 'merge'

    # Test suites
    _suites = (
          ('MergeSystemSuits', [
              'testMergeTakeLocalSettings',
              'testMergeTakeRemoteSettings',
              'testRestartOneServer',
              'testMergeCloudWithLocal',
              'testMergeCloudSystems',
              'testCloudMergeAfterDisconnect'
          ]),
      )

    class Server:

        def __init__(self, sysName, settings, user, password):
            self.sysName = sysName
            self.settings = settings
            self.user = user
            self.password = password

        def change_credentials(self, other):
            self.user = other.user
            self.password = other.password

    def setUp(self):
        FuncTestCase.setUp(self)
        self.serverAddr1 = self.sl[0]
        self.serverAddr2 = self.sl[1]
        self.mergeTimeout = config.getint("General", "mergeTestTimeout")
        self.user = config.get("General", "username")
        self.password = config.get("General", "password")
        self.client = Client(self.user, self.password)
        self.sysName1 = uuid.uuid4().hex
        self.sysName2 = uuid.uuid4().hex
        self.servers = {
          self.serverAddr1:
            self.Server(self.sysName1, SYSTEM_SETTINGS_1, self.user, self.password),
          self.serverAddr2:
              self.Server(self.sysName2, {}, self.user, self.password)}

    def __prepareInitialState(self, skipInitSrvs = []):
        tlog(LOGLEVEL.INFO, "Prepare initial state...")
        try:
            self._prepare_test_phase(self._stop_and_init)
        except Exception:
            type(self)._initFailed = True
            raise

        # Assign new system names
        for srv,info in self.servers.items():
            if not srv in skipInitSrvs:
                settings = info.settings.copy()
                settings['password'] = info.password
                settings['systemName'] = info.sysName
                response = self.client.httpRequest(
                    srv, "api/setupLocalSystem",
                    headers={'Content-Type': 'application/json'},
                    data=json.dumps(settings))
                self.checkResponseError(response, "api/setupLocalSystem")
                # Check setupLocalSystem's settings
                if info.settings.get("systemSettings"):
                    response = self.client.httpRequest(
                        srv, "api/systemSettings")
                    self.__checkSettings(response.data,
                        info.settings.get("systemSettings"))
            time.sleep(1.0)

        tlog(LOGLEVEL.INFO, "Prepare initial state done")

    # Change boolean global settings
    def __changeBoolSettings(self, srv, name):
        tlog(LOGLEVEL.INFO, "Change system setting '%s' on '%s'..." % (name, srv))
        response = self.client.httpRequest(
          srv, "api/systemSettings")
        val = str2bool(response.data['reply']['settings'][name])
        kw = { name: bool2str(not val) }
        response = self.client.httpRequest(
          srv, "api/systemSettings", **kw)
        self.checkResponseError(response, 'api/systemSettings')
        response = self.client.httpRequest(
          srv, "api/systemSettings")
        self.assertEqual(response.data['reply']['settings'][name], bool2str(not val),
                         "%s changes")
        tlog(LOGLEVEL.INFO, "Change system setting '%s' on '%s (%s->%s) done" % (name, srv,
                                                                     bool2str(val),
                                                                     bool2str(not val)))
        return val


    def __checkSettings(self, data, settings):
        got_settings = data['reply']['settings']
        for name, val in settings.items():
            if type(val) is bool:
                val = bool2str(val)
            self.assertEqual(got_settings.get(name), val,
              "Unexpected system settings '%s' value (got != expected): '%s' != '%s'" % \
                 (name, got_settings[name], val))

    # Merge srv2 to srv1
    def __mergeSystems(self,
                       takeRemoteSettings = False,
                       apiErrorCode = 0,
                       apiErrorString = ''):
        tlog(LOGLEVEL.INFO, "Systems merging start...")
        srvInfo1 = self.servers[self.serverAddr1]
        srvInfo2 = self.servers[self.serverAddr2]
        srvClient1 = Client(srvInfo1.user, srvInfo1.password)
        srvClient2 = Client(srvInfo2.user, srvInfo2.password)
        # api/getNonce doesn't require credentials
        # So, we can use any existing user for the request
        response = srvClient1.httpRequest(self.serverAddr1, "api/getNonce")
        self.checkResponseError(response, "api/getNonce")
        nonce = response.data["reply"]["nonce"]
        realm = response.data["reply"]["realm"]
        response = srvClient2.httpRequest(self.serverAddr2, "api/mergeSystems",
           url = "http://%s" % self.serverAddr1,
           getKey=generateKey('GET', srvInfo1.user, srvInfo1.password, nonce, realm),
           postKey=generateKey('POST', srvInfo1.user, srvInfo1.password, nonce, realm),
           takeRemoteSettings=bool2str(takeRemoteSettings))
        self.checkResponseError(response, "api/mergeSystems",
                                apiErrorCode = apiErrorCode,
                                apiErrorString = apiErrorString)
        if not (apiErrorCode or apiErrorString):
            srvInfo2.change_credentials(srvInfo1)
        tlog(LOGLEVEL.INFO, "System merging done")

    def __waitMergeDone(self):
        tlog(LOGLEVEL.INFO, "Systems merging (wait cycle) start...")
        start = time.time()
        srvInfo1 = self.servers[self.serverAddr1]
        srvInfo2 = self.servers[self.serverAddr2]
        tlog(LOGLEVEL.INFO, "Use following credentials:")
        for s, si in self.servers.items():
            tlog(LOGLEVEL.INFO, "  %s: user: '%s', pwd: '%s'" %
                 (s, si.user, si.password))
        while True:
            srvClient1 = Client(srvInfo1.user, srvInfo1.password)
            srvClient2 = Client(srvInfo2.user, srvInfo2.password)
            response1 = srvClient1.httpRequest(self.serverAddr1, "api/systemSettings")
            response2 = srvClient2.httpRequest(self.serverAddr2, "api/systemSettings")
            remain = self.mergeTimeout - (time.time() - start)
            if remain <= 0:
                self.assertEqual(response1.status, 200)
                self.assertEqual(response2.status, 200)
                self.compare(response1.data["reply"], response2.data["reply"], 'after merge')
                tlog(LOGLEVEL.INFO, "Systems merging (wait cycle) done")
                return response1.data, response2.data
            if response1.status == response2.status == 200 and \
               self.isEqual(response1.data, response2.data):
                tlog(LOGLEVEL.INFO, "Systems merging (wait cycle) done")
                return response1.data, response2.data
            time.sleep(1.0)

    def __setupCloud(self, srv, systemName):
        cloudClient = Client(CLOUD_USER_NAME, CLOUD_USER_PWD)
        srvInfo = self.servers[srv]
        client = Client(srvInfo.user, srvInfo.password)
        result = cloudClient.httpRequest(
          CLOUD_SERVER,
          'cdb/system/bind',
          name=systemName,
          customization=DEFAULT_CUSTOMIZATION)
        self.checkResponseError(result, "cdb/system/bind")
        settings = srvInfo.settings.copy()
        settings['systemName'] = systemName
        settings['cloudAuthKey'] = result.data['authKey']
        settings['cloudSystemID'] = result.data['id']
        settings['cloudAccountName'] = CLOUD_USER_NAME
        response = client.httpRequest(
          srv,
          "api/setupCloudSystem",
          headers={'Content-Type': 'application/json'},
          data=json.dumps(settings))
        self.checkResponseError(response, "api/setupCloudSystem")
        srvInfo.user = CLOUD_USER_NAME
        srvInfo.password = CLOUD_USER_PWD

    def __checkServerConnectedToCloud(self, srv):
        srvInfo = self.servers[srv]
        client = Client(srvInfo.user, srvInfo.password)
        response = client.httpRequest(self.serverAddr1, "api/getNonce")
        self.checkResponseError(response, "api/getNonce")
        nonce = response.data["reply"]["nonce"]
        realm = response.data["reply"]["realm"]
        response =client.httpRequest(
            self.serverAddr1,
            'api/moduleInformationAuthenticated',
            getKey=generateKey('GET', client.user, client.password, nonce, realm))
        self.checkResponseError(response, "api/moduleInformationAuthenticated")
        self.assertTrue(bool(response.data['reply']['cloudSystemId']))
 
    # wait cloud credentials
    def __waitCredentials(self, srv):
        tlog(LOGLEVEL.INFO, "Cloud credentials (wait cycle) start...")
        srvInfo = self.servers[srv]
        client = Client(srvInfo.user, srvInfo.password)
        start = time.time()
        while True:
            response = client.httpRequest(srv, "ec2/testConnection")
            if response.status != 200:
                remain = SETUP_CLOUD_TIMEOUT - (time.time() - start)
                if remain <= 0:
                    self.fail("Can't get cloud credentials for '%s'" % srv)
                time.sleep(5.0)
            else:
                break
        tlog(LOGLEVEL.INFO, "Cloud credentials (wait cycle) done")


    # Test cases

    def testMergeTakeLocalSettings(self):
        "Merge server2 to server1, take local setting"
        # Start two servers without predefined systemName
        # and move them to working state

        self.__prepareInitialState()

        # On each server update some globalSettings to different values
        oldArecontRtspEnabled = \
          self.__changeBoolSettings(self.serverAddr1, 'arecontRtspEnabled')
        newAuditTrailEnabled = \
           not self.__changeBoolSettings(self.serverAddr2, 'auditTrailEnabled')

        # Merge systems (takeRemoteSettings = false)
        self.__mergeSystems()
        data1, data2 = self.__waitMergeDone()
        self.__checkSettings(data2,
          {'arecontRtspEnabled': oldArecontRtspEnabled,
           'auditTrailEnabled': newAuditTrailEnabled})

        # Ensure both servers are merged and sync
        newArecontRtspEnabled = \
          not self.__changeBoolSettings(self.serverAddr1, 'arecontRtspEnabled')
        data1, data2 = self.__waitMergeDone()
        self.__checkSettings(data2, {'arecontRtspEnabled': newArecontRtspEnabled})

    def testMergeTakeRemoteSettings(self):
        "Merge server2 to server1, take remote settings"
        # Start two servers without predefined systemName
        # and move them to working state
        self.__prepareInitialState()

        # On each server update some globalSettings to different values
        newArecontRtspEnabled = \
          not self.__changeBoolSettings(self.serverAddr1, 'arecontRtspEnabled')
        newAuditTrailEnabled = \
           not self.__changeBoolSettings(self.serverAddr2, 'auditTrailEnabled')

        # Merge systems (takeRemoteSettings = true)
        self.__mergeSystems(True)
        data1, data2 = self.__waitMergeDone()
        self.__checkSettings(
          data2,
          {'arecontRtspEnabled': newArecontRtspEnabled,
           'auditTrailEnabled': newAuditTrailEnabled})

        # Ensure both servers are merged and sync
        newAuditTrailEnabled = \
          not self.__changeBoolSettings(self.serverAddr1, 'auditTrailEnabled')
        data1, data2 = self.__waitMergeDone()
        self.__checkSettings(data2, {'auditTrailEnabled': newAuditTrailEnabled})


    def testRestartOneServer(self):
        "Merge after restarting a server"
        srv2Guid = self.guids[1]
        srv2Box = self.hosts[1]

        # Stop Server2 and remove its database
        self._mediaserver_ctl(srv2Box, 'safe-stop')
        execVBoxCmd(srv2Box, "rm", "-rvf", os.path.join(MEDIA_SERVER_DIR, 'var', '*sqlite*'))

        # Remove Server2 from database on Server1
        response = self.client.httpRequest(
          self.serverAddr1,
          "ec2/removeResource",
          headers={'Content-Type': 'application/json'},
          data=json.dumps({'id': srv2Guid}))
        self.checkResponseError(response, 'ec2/removeResource')

        # Start server 2 again and move it from initial to working state
        self._mediaserver_ctl(srv2Box, 'safe-start')
        self._wait_servers_up()
        response = self.client.httpRequest(
          self.serverAddr2,
          "api/setupLocalSystem",
          systemName = self.sysName2,
          password = self.password,
          format = "json")
        self.checkResponseError(response, "api/setupLocalSystem")

        # Merge systems (takeRemoteSettings = false)
        self.__mergeSystems()
        self.__waitMergeDone()

        # Ensure both servers are merged and sync
        newArecontRtspEnabled = \
          not self.__changeBoolSettings(self.serverAddr1, 'arecontRtspEnabled')
        data1, data2 = self.__waitMergeDone()
        self.__checkSettings(data2, {'arecontRtspEnabled': newArecontRtspEnabled})

    def testMergeCloudWithLocal(self):
        "Merge cloud system with local"
        self.__prepareInitialState([self.serverAddr1])
        # Setup cloud and wait new cloud credentials
        self.__setupCloud(self.serverAddr1, self.sysName1)
        self.__waitCredentials(self.serverAddr1)
        self.__checkServerConnectedToCloud(self.serverAddr1)

        # Merge systems (takeRemoteSettings = False) -> Error
        self.__mergeSystems(False,
            apiErrorCode = 3,
            apiErrorString = 'DEPENDENT_SYSTEM_BOUND_TO_CLOUD')

        # Merge systems (takeRemoteSettings = true)
        self.__mergeSystems(True)
        self.__waitCredentials(self.serverAddr2)
        self.__waitMergeDone()

    
    def testMergeCloudSystems(self):
        "Merge two cloud systems"
        self.__prepareInitialState([self.serverAddr1, self.serverAddr2])
        
        # Setup cloud and wait new cloud credentials
        self.__setupCloud(self.serverAddr1, self.sysName1)
        self.__setupCloud(self.serverAddr2, self.sysName2)
        self.__waitCredentials(self.serverAddr1)
        self.__waitCredentials(self.serverAddr2)

        # Merge 2 cloud systems (takeRemoteSettings = False) -> Error
        self.__mergeSystems(False,
            apiErrorCode = 3,
            apiErrorString = 'BOTH_SYSTEM_BOUND_TO_CLOUD')
        self.__mergeSystems(True,
            apiErrorCode = 3,
            apiErrorString = 'BOTH_SYSTEM_BOUND_TO_CLOUD')

        # Test default (admin) user disabled
        srvInfo = self.servers[self.serverAddr1]
        client = Client(srvInfo.user, srvInfo.password)
        response = client.httpRequest(self.serverAddr1, "ec2/getUsers")
        self.checkResponseError(response, "ec2/getUsers")
        adminGuid = getAdminGuid(response)
        self.assertTrue(adminGuid)
        response = client.httpRequest(
            self.serverAddr1, "ec2/saveUser",
            headers={'Content-Type': 'application/json'},
            data=json.dumps({'id': adminGuid,
                             'isEnabled': True}))
        self.checkResponseError(response, "ec2/saveUser", status = 403)
       
        
    def testCloudMergeAfterDisconnect(self):
        "Merge after disconnect from cloud"
        self.__prepareInitialState([self.serverAddr1, self.serverAddr2])

        # Setup cloud and wait new cloud credentials
        self.__setupCloud(self.serverAddr1, self.sysName1)
        self.__setupCloud(self.serverAddr2, self.sysName2)
        self.__waitCredentials(self.serverAddr1)
        self.__waitCredentials(self.serverAddr2)

        # Check setupCloud's settings on Server1
        self.client = Client(CLOUD_USER_NAME, CLOUD_USER_PWD)
        srvInfo1 = self.servers[self.serverAddr1]
        response = self.client.httpRequest(
          self.serverAddr1, "api/systemSettings")
        self.__checkSettings(
          response.data,
          srvInfo1.settings.get("systemSettings"))

        # Disconnect Server2 from cloud
        newSrv2Pwd = 'new_password'
        response = self.client.httpRequest(
          self.serverAddr2,
          'api/detachFromCloud',
          password = newSrv2Pwd)
        self.checkResponseError(response, 'api/detachFromCloud')
        self.servers[self.serverAddr2].user = self.user
        self.servers[self.serverAddr2].password = newSrv2Pwd

        # Merge systems (takeRemoteSettings = true)
        self.__mergeSystems(True)
        self.__waitCredentials(self.serverAddr2)
        self.__waitMergeDone()

        # Ensure both servers are merged and sync
        newAuditTrailEnabled = \
          not self.__changeBoolSettings(self.serverAddr1, 'auditTrailEnabled')
        data1, data2 = self.__waitMergeDone()
        self.__checkSettings(data2, {'auditTrailEnabled': newAuditTrailEnabled})
