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

    SAVE_USER_DATA =  {
        "cryptSha512Hash": "",
        "digest": "",
        "hash": "",
        "isAdmin": False,
        "isEnabled": True,
        "isLdap": False,
        "parentId": "{00000000-0000-0000-0000-000000000000}",
        "realm": "",
        "typeId": "{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}",
        "url": "" }

    helpStr = 'Single server instance test'
    _test_name = 'Single server instance'
    _test_key = 'instance'
    # Test suites
    _suites = ( ('SingeServerInstanceSuits', [
        'testCreateAndCheck',
        'testNonExistUserGroup']), )

    def setUp(self):
        MediaServerInstanceTest.setUp(self)
        ClientMixin.setUp(self)

    def tearDown(self):
        MediaServerInstanceTest.tearDown(self)
        ClientMixin.tearDown(self)

    def __createUserData(self, guid, name,
                         email, role = None,
                         permissions = "GlobalAdminPermission"):
        userData = self.SAVE_USER_DATA.copy()
        userData['id'] = guid
        userData['name'] = name
        userData['email'] = email
        userData['permissions'] = permissions
        if role:
            userData['userRoleId'] = role
        return userData

    def __createUser(self,
                     name, email, role = None,
                     permissions = "GlobalAdminPermission"):
        userGuid = generateGuid()
        userData = self.__createUserData(
            userGuid, name, email, role, permissions)
                
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           headers={'Content-Type': 'application/json'},
           data=json.dumps(userData))

        return userData

    def __removeUser(self, userGuid):
        self.sendAndCheckRequest(
            self.server.address, "ec2/removeUser",
            headers={'Content-Type': 'application/json'},
            data=json.dumps({'id': userGuid}))

    # https://networkoptix.atlassian.net/browse/VMS-2246
    def testCreateAndCheck(self):
        "Create and check user"
        userData = self.__createUser("user1", "user1@example.com", permissions = "2432")
        
        # To make sure 
        time.sleep(2.0)

        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")

        expData = userData.copy()
        expData["permissions"] = "GlobalViewArchivePermission|GlobalManageBookmarksPermission|0x80"

        self.assertHasItem(expData, response.data, 'User#1')

        self.__removeUser(userData['id'])

        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")
        
        self.assertHasNotItem(expData, response.data, 'User#1')


    def testNonExistUserGroup(self):
        "Unexisting user role"
        unexistsUserRole = generateGuid()
        userData1 = self.__createUser("user2", "user2@example.com")
        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")
        self.assertHasItem(userData1, response.data, 'User#1')
        userGuid2 = generateGuid()
        userData2 = self.__createUserData(
            userGuid2, "user3", "user3@example.com", role = unexistsUserRole)

        # Try link exists user to unexisting role
        userData1_invalid = userData1.copy()
        userData1_invalid['userRoleId'] = unexistsUserRole
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           status=403,
           headers={'Content-Type': 'application/json'},
           data=json.dumps(userData1_invalid))

        # Try create new user to unexisting role
        self.sendAndCheckRequest(
           self.server.address, "ec2/saveUser",
           status=403,
           headers={'Content-Type': 'application/json'},
           data=json.dumps(userData2))

        response = self.sendAndCheckRequest(
            self.server.address, "ec2/getUsers")

        self.assertHasItem(userData1, response.data, 'User#1')
        self.assertHasNotItem({'id': userGuid2}, response.data, 'User#2')
        

        
        

        

