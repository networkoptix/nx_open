# $Id$
# Artem V. Nikitin
# Single server test sample

import time, os, json

from pycommons.MockClient import ClientMixin
from pycommons.SingleServerTest import MediaServerInstanceTest
from pycommons.Logger import LOGLEVEL, log
from pycommons.Config import config
from pycommons.Utils import generateGuid

class InstanceTest(MediaServerInstanceTest, ClientMixin):
    "Single server instance test"

    helpStr = 'Single server instance test'
    _test_name = 'Single server instance'
    _test_key = 'instance'
    # Test suites
    _suites = ( ('SingeServerInstanceSuits', ['testCreateAndCheck', ]), )

    def setUp(self):
        MediaServerInstanceTest.setUp(self)
        ClientMixin.setUp(self)

    def tearDown(self):
        MediaServerInstanceTest.tearDown(self)
        ClientMixin.tearDown(self)

    # https://networkoptix.atlassian.net/browse/VMS-2246
    def testCreateAndCheck(self):
        "Create and check user"
        userGuid = generateGuid()
               
        SAVE_USER_DATA = {
            "cryptSha512Hash": "",
            "digest": "",
            "email": "user1@example.com",
            "hash": "",
            "id": userGuid,
            "isAdmin": False,
            "isEnabled": True,
            "isLdap": False,
            "name": "user1",
            "parentId": "{00000000-0000-0000-0000-000000000000}",
            "permissions": "2432",
            "realm": "",
            "typeId": "{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}",
            "url": "" }
        
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           headers={'Content-Type': 'application/json'},
           data=json.dumps(SAVE_USER_DATA))
        
        # To make sure 
        time.sleep(2.0)

        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")

        expData = SAVE_USER_DATA.copy()
        expData["permissions"] = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"

        self.assertHasItem(expData, response.data, 'User#1')

        self.sendAndCheckRequest(
            self.server.address, "ec2/removeUser",
            headers={'Content-Type': 'application/json'},
            data=json.dumps({'id': userGuid}))

        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")
        
        self.assertHasNotItem(expData, response.data, 'User#1')

        
        

        

