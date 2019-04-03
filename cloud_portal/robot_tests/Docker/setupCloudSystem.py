#This file is for setting up and adding a new system to cloud

import requests
import time
import sys

SERVER_URL = "http://localhost:7001"
CLOUD_URL = "https://cloud-test.hdw.mx"
SYSTEM_NAME = sys.argv[1] if len(sys.argv)>1 else "kyle-VirtualBox"
CUSTOMIZATION = "default"
CLOUD_USER = "noptixautoqa+owner@gmail.com"
CLOUD_PASS = "qweasd 123"
DEFAULT_LOGIN = "admin"
DEFAULT_PASSWORD = "admin"


# This returns the cloudAuthKey and the cloudSystemID we will need for /api/setupCloudSystem
r = requests.post(CLOUD_URL + "/cdb/system/bind", 
	json={ 
		"name": SYSTEM_NAME, 
		"customization": CUSTOMIZATION
		}, 
	auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
print r.status_code
jsonData = r.json()


# This takes the default login of admin/admin as the server should not already be setup
# If the server is already setup, then you need to run a different command
s = requests.post("{}/api/setupCloudSystem".format(SERVER_URL), 
	json={
		"cloudAuthKey": jsonData["authKey"], 
		"systemName": jsonData["name"], 
		"cloudSystemID": jsonData["id"], 
		"cloudAccountName": jsonData["ownerAccountEmail"]
		}, 
	auth=requests.auth.HTTPDigestAuth(DEFAULT_LOGIN, DEFAULT_PASSWORD))
print s.status_code

#This creates the "Client Custom" user role and returns the userRoleId
t = requests.post("{}/ec2/saveUserRole".format(SERVER_URL),
	json={
	    "name": "Client Custom",
	    "permissions": "GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"
	    },
	auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
print t.status_code
jsonDataT = t.json()

#List of users and their permission types. Client Custom gets Custom for now
userList = {"viewer":"viewer",
			"advviewer":"advancedViewer",
			"liveviewer":"liveViewer",
			"notowner":"viewer",
			"admin":"cloudAdmin",
			"custom":"Custom",
			"clientcustom":"Custom"}

#Go through each of the users and share them with the system
for user, permission in userList.iteritems():
	u = requests.post("{}/cdb/system/share".format(CLOUD_URL),
		json={
			"systemId" : jsonData["id"],
			"accessRole": permission,
			"accountEmail"  : "noptixautoqa+{}@gmail.com".format(user),
			},
		auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
	print u.status_code

#This loop gets the users list once it is at least 8 long
vLen = 0
while vLen < 9:
	v = requests.get("{}/ec2/getUsers".format(SERVER_URL),
		auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
	jsonDataV = v.json()
	vLen = len(jsonDataV)
	print "Waiting for user list to update..."
	time.sleep(1)

#This takes the Client Custom user and changes their permissions from Custom to Client Custom
w = requests.post("{}/ec2/saveUser".format(SERVER_URL),
	json={
		"isCloud": True,
		"id": jsonDataV[7]["id"],
		"userRoleId": jsonDataT["id"]
		},
	auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
print w.status_code
