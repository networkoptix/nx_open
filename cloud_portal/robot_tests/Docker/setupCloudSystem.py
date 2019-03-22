#This file is for setting up and adding a new system to cloud

import requests

SERVER_URL = "http://localhost:7001"
CLOUD_URL = "https://cloud-test.hdw.mx"
SYSTEM_NAME = "kyle-VirtualBox"
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


t = requests.post("{}/ec2/saveUserRole".format(SERVER_URL),
	json={
	    "name": "Client Custom",
	    "permissions": "GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"
	    },
	auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
print t.status_code


userList = {"viewer":"viewer",
			"advviewer":"advancedViewer",
			"liveviewer":"liveViewer",
			"notowner":"viewer",
			"admin":"cloudAdmin",
			"custom":"Custom",
			"clientcustom":"Client%20Custom"}
for user, permission in userList.iteritems():
	print user + " : " + permission
	u = requests.post("{}/cdb/system/share".format(CLOUD_URL, jsonData["id"]),
		json={
			"systemId" : jsonData["id"],
			"accessRole": permission,
			"accountEmail"  : "noptixautoqa+{}@gmail.com".format(user),
			},
		auth=requests.auth.HTTPDigestAuth(CLOUD_USER, CLOUD_PASS))
	print u.text