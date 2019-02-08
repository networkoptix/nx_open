# -*- coding: utf-8 -*-
""" Big Test Base creator.
    Fills server db with many serversm cameras, users, layouts, etc.
"""
__author__ = 'Danil Lavrentyuk'

#import argparse
import base64
import json
import random
import requests
import sys
import traceback

sys.path.append('..')  # FIXME create an abs path
from generator import BasicGenerator, MediaServerGenerator, CameraDataGenerator, UserDataGenerator

ADDR = "127.0.0.1:7003"
AUTH = ('admin', 'qweasd123')

WEB_PAGES = (  # several pages used as a part of layouts content
    ('http://grib-info.ru/syedobnie/berezovyj.html', "Berezovyj"),
    ('http://grib-info.ru/syedobnie/borovik.html', "Borovik"),
    ('http://grib-info.ru/syedobnie/podosinovik.html', "Podosinovk"),
    ('http://grib-info.ru/syedobnie/dubovik.html', "Dubovik"),
    ('http://grib-info.ru/syedobnie/lisichki.html', "Lisichka"),
    ('http://grib-info.ru/syedobnie/kolpak-kolchatyj.html', "Kolpak Kolchatyj"),
    ('http://grib-info.ru/syedobnie/podberezovik.html', "Podberezovik"),
    ('http://grib-info.ru/syedobnie/podgruzdok-suxoj.html', "Podgruzdok Suhoj"),
    ('http://grib-info.ru/syedobnie/ryzhiki.html', "Ryzhik"),
    ('http://grib-info.ru/syedobnie/kak-vyglyadyat-griby-navozniki.html', "Navoznik"),
    ('http://grib-info.ru/syedobnie/gruzdi-2.html', "Gruzd"),
    ('http://grib-info.ru/syedobnie/zontiki.html', "Zontik"),
    ('http://grib-info.ru/syedobnie/letnie-opyata.html', "Openok Lentij"),
    ('http://grib-info.ru/syedobnie/syroezhki-2.html', "Zyroezhka"),
)

# This images aren't stored in Mercurial
# I've left them on smb://10.0.2.106//public/BigTestBase,
# think you'd choose a better place for them.
# ( Danil )
ToStore = (
    {'path': 'wallpapers/image%02d.jpeg', 'file': 'bgimage.jpeg'},
    {'path': 'wallpapers/image%02d.png', 'file': 'bgimage.png'},
)

NUM_SERVERS = 20
CAMERAS_PER_SERVER = 10
NUM_USERS = 100
NUM_SHARED_LAYOUTS = 50
SHARED_PER_USER = 10  # and per role too
RES_PER_USER = 20
LAYOUTS_PER_USER =  10 # private layouts
RES_PER_LAYOUT = 10
NUM_ROLES = 20
USERS_PER_ROLE = 10
RES_PER_ROLE = 20

NUM_BGIMAGES = 25  # per each format: jpeg and png

Servers = dict()
Cameras = dict()
WebPages = dict()
AllResIds = set()
Users = dict()
PvtLayouts = dict()  # user -> {layoutId -> data}
SharedLayouts = dict()  # uid -> (json_data, ids_set)
Rules = dict()
UserRoles = dict()  # uid -> (json_data, resIds)
Images = []


def sendRequest(url, data, notify=False, noparsae=False, allowEmpty=False):
    response = None
    try:
        if notify:
            print("Requesting " + url)
            if data:
                print("data = " + data)
        if data is None:
            response = requests.get(url, auth=AUTH)
        else:
            response = requests.post(
                url, data=data, headers={'Content-Type': 'application/json'}, auth=AUTH)
#        if response.getcode() != 200:
        if response.status_code != 200:
            raise Exception("HTTP error code %s at %s with data %s\nResponse content: '%s'" %
                            (response.status_code, url, data, response.text))
        if noparsae:
            return response
        if response.text == '':
            if allowEmpty:
                return ''
            else:
                raise Exception("%s returned empty response" % url)
        # ok, let it raise in case of bad json
        respData = json.loads(response.text)
        if type(respData) == dict and respData.get('error', 0):
            print("Failed on data: %s" % (data,))
            raise Exception("%s returned error: %s" % (url, respData))
        return respData
    except Exception as err:
        print("Failed at %s with data %s" % (url, data))
        raise


def ec2Request(func, data, notify=False):
    return sendRequest("http://%s/ec2/%s" % (ADDR, func), data, notify)


class UniqGenerator(BasicGenerator):
    _uniqId = set()  # do not reassign, it's a class variable
    _uniqIP = set()  # do not reassign, it's a class variable

    def generateRandomId(self):
        while True:
            uid = super(UniqGenerator, self).generateRandomId()
            if uid not in self._uniqId:
                self._uniqId.add(uid)
                return uid

    def generateIpV4(self):
        while True:
            ip = super(UniqGenerator, self).generateIpV4()
            if ip not in self._uniqIP:
                self._uniqIP.add(ip)
                return ip


class ServerGenerator(MediaServerGenerator, UniqGenerator):
    def _genServerIPs(self):
        return "192.168.%s.%s;10.0.%s.%s" % (
            random.randint(0,31),random.randint(0,255),random.randint(0,31),random.randint(0,255))

    def createServers(self, number):
        servList = self.generateMediaServerData(number)
        for data, id in servList:
            ec2Request("saveMediaServer", data)
            print("Created server %s" % (id,))
            Servers[id] = data
        AllResIds.update(Servers.keys())


def rmBrackets(text):
    p1, p2 = 0, len(text)
    while text[p1].isspace():
        p1 += 1
    if text[p1] == '[':
        p1 += 1
    while text[p2-1].isspace():
        p2 -= 1
    if text[p2-1] == ']':
        p2 -= 1
    return text[p1:p2]


class CameraGenerator(CameraDataGenerator, UniqGenerator):
    _template = rmBrackets(CameraDataGenerator._metaTemplate % ('preferredServerId',))

    def _getServerUUID(self, addr):  # here we pass id here, not an address
        return addr

    def createCameras(self):
        for sId,sData in Servers.iteritems():
            alldata = []
            for data, cId in self.generateCameraData(CAMERAS_PER_SERVER, sId):
                #print("Generated camera: %s: %s" % (cId, data))
                alldata.append(data)
                Cameras[cId] = data
            ec2Request("saveCameras", "[%s]" % (','.join(alldata),))
            print("%s cameras created" % (len(alldata),))
        AllResIds.update(Cameras.keys())


class UserGenerator(UserDataGenerator, UniqGenerator):

    def createUsers(self, number, role=''):
        permissions = "GlobalCustomUserPermission" if role == '' else ''
        created = list()
        for data, uId in self.generateUserData(number, permissions, role):
            ec2Request("saveUser", data)
            Users[uId] = data
            created.append(uId)
#            print("User created: %s: %s" % (uId, data))
            print("User created: %s" % (uId,))
        return created


class WebPageGenerator(UniqGenerator):
    _template = """{
        "id": "%s",
        "url": "%s",
        "name": "%s"
    }
    """

    def createWebPages(self):
        for rec in WEB_PAGES:
            url, name = rec
            uid = self.generateRandomId()
            data = self._template % (uid, url, name)
            ec2Request("saveWebPage", data)
            WebPages[uid] = data
        print("%s web pages created" % (len(WEB_PAGES),))
        AllResIds.update(WebPages.keys())



class LayoutGenerator(UniqGenerator):
    _template = """{
        "id": "%(id)s",
        "parentId": "%(parent)s",
        "name": "%(name)s",
        "items": [%(items)s],
        "locked": false,
        "backgroundImageFilename": "%(bgfile)s",
        "backgroundWidth": 5,
        "backgroundHeight": 5,
        "backgroundOpacity": 0.5
    }
    """

    _itemTemplate = """{
        "id": "%s",
        "flags": 0,
        "left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0,  "rotation": 0.0,
        "resourceId": "%s",
        "resourcePath": "",
        "zoomLeft": "", "zoomTop": "", "zoomRight": "", "zoomBottom": "",
        "zoomTargetId": "",
        "displayInfo": false
    }
    """

    def _makeLayoutContent(self):
        content = random.sample(AllResIds, RES_PER_LAYOUT)
#        servnum = random.randint(1, RES_PER_LAYOUT-2)
#        content.extend(random.sample(Servers.keys(), servnum))
#        camnum = random.randint(1, RES_PER_LAYOUT - servnum -1)
#        content.extend(random.sample(Cameras.keys(), camnum))
#        pagenum = RES_PER_LAYOUT - servnum - camnum
#        content.extend(random.sample(WebPages.keys(), pagenum))
#        print "%s servers, %s cameras, %s web pages" % (servnum, camnum, pagenum)
        return (', '.join(
            self._itemTemplate % (self.generateRandomId(), resId) for resId in content
        ), content)

    def createLayouts(self, number, owner=None, resPool=AllResIds):
        if owner:
            print("createLayouts for %s" % (owner,))
        #resIds = set()
        for _ in xrange(number):
            content = set(random.sample(resPool, RES_PER_LAYOUT))
            contentStr = ', '.join(
                self._itemTemplate % (self.generateRandomId(), resId) for resId in content)
            uid = self.generateRandomId()
            data = self._template % {
                'id' : uid,
                'parent': "" if owner is None else owner,
                'name': self.generateLayoutName(),
                'items': contentStr,
                'bgfile': random.choice(Images),
            }
            response = ec2Request("saveLayout", data)
            if owner is None:
                SharedLayouts[uid] = (data, content)
            else:
                PvtLayouts.setdefault(owner, dict())[uid] = data
                #resIds.update(content)
        print("%s %slayouts created" % (number, 'shared ' if not owner else ''))
        #return resIds



class UserRoleGenerator(UniqGenerator):
    _template = """{
        "id": "%s",
        "name": "%s",
        "permissions": ""
    }"""

    def createUserRoles(self, uGen, lGen):
        for r in xrange(NUM_ROLES):
            uid = self.generateRandomId()
            data = self._template % (uid, self.generateUserRoleName())
            resp = ec2Request("saveUserRole", data)
            available = random.sample(AllResIds, RES_PER_ROLE)
            UserRoles[uid] = (data, available)
            print("Role %s created" % (uid,))
            users = uGen.createUsers(USERS_PER_ROLE, uid)  # it add each user to the Users list, may be it's not good
            for userId in users:
                lGen.createLayouts(LAYOUTS_PER_USER, userId, available)


def loadRealServers():
    data = ec2Request('getMediaServersEx', None)
    for server in data:
        print("Found real server %s" % server['id'])
        Servers[server['id']] = json.dumps(server)


def setAccessRights(userId, resList):
        rights = """{
            "userId": "%s",
            "resourceIds": [%s]
        }""" % (userId, ",\n ".join('"%s"' % rId for rId in resList))
        #print rights
        resp = ec2Request("setAccessRights", rights)


def storeFiles(number):
    for fdesk in ToStore:
        img = base64.b64encode(open(fdesk['file'], "rb").read())
        data = {
                'data': img,
        }
        for n in xrange(number):
            data['path'] = fdesk['path'] % n
            ec2Request('addStoredFile', json.dumps(data))
            data['path'] = data['path'].split('/',1)[1]
            Images.append(data['path'])
        print("%s %s images loaded" % (number, fdesk['path'].split('.')[-1]))


def main():
    loadRealServers()
    storeFiles(NUM_BGIMAGES)
    ServerGenerator().createServers(NUM_SERVERS)
    CameraGenerator().createCameras()
    WebPageGenerator().createWebPages()

    uGen = UserGenerator()
    uGen.createUsers(NUM_USERS)
    lGen = LayoutGenerator()
    lGen.createLayouts(NUM_SHARED_LAYOUTS)

    for userId in Users.iterkeys():
        available = random.sample(AllResIds, RES_PER_USER)
        # layouts
        lGen.createLayouts(LAYOUTS_PER_USER, userId, available)
        sh = random.sample(SharedLayouts.keys(), SHARED_PER_USER)
        print("User %s, shared layouts %s" % (userId, sh))
        setAccessRights(userId, available + sh)

    UserRoleGenerator().createUserRoles(uGen, lGen)
    for roleId, roleData in UserRoles.iteritems():
        #print "Granting layouts for role %s" % (roleId,)
        sh = random.sample(SharedLayouts.keys(), SHARED_PER_USER)
        print("Role %s, shared layouts %s" % (roleId, sh))
        setAccessRights(roleId, roleData[1] + sh)

if __name__ == "__main__":
    main()
