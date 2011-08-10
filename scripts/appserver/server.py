#!/usr/bin/env python

from restclient import *
from StringIO import StringIO
from pprint import pprint

from xml.dom import minidom
import socket, sys

SERVER = "http://localhost:8000/api/"
CREDENTIALS = ("api", "123")

class Object(object):
    def load_from_dict(self, xdict):
        for xstr, attr in self.attr_mapping.items():
            if xstr in xdict:
                self.__setattr__(attr, xdict[xstr])

class ResourceType(Object):
    def __init__(self):
        Object.__init__(self)

        self.attr_mapping = {'id' : 'xid', 'name' : 'name'}

        self.xid = ''
        self.name = ''

    def __repr__(self):
        sio = StringIO()
        print >> sio, 'ID: %s\t' % self.xid,
        print >> sio, ', Name: %s\t' % self.name,
        return sio.getvalue()

class Camera(Object):
    def __init__(self):
        Object.__init__(self)

        self.attr_mapping = {'id' : 'xid', 'name' : 'name', 'mac' : 'mac', 'url' : 'url', 'login' : 'login', 'password' : 'password'}

        self.xid = ''
        self.name = ''
        self.mac = ''
        self.url = ''
        self.login = ''
        self.password = ''

    def __repr__(self):
        sio = StringIO()
        print >> sio, 'ID: %s\t' % self.xid,
        print >> sio, ', Name: %s\t' % self.name,
        print >> sio, ', Mac: %s\t' % self.mac,
        print >> sio, ', Url: %s\t' % self.url,
        print >> sio, ', Login: %s\t' % self.login,
        print >> sio, ', Password: %s\t' % self.password,
        return sio.getvalue()
        
class Server(Object):
    def __init__(self):
        Object.__init__(self)

        self.attr_mapping = {'id' : 'xid', 'name' : 'name'}

    def __repr__(self):
        sio = StringIO()
        print >> sio, 'ID: %s\t' % self.xid,
        print >> sio, ', Name: %s\t' % self.name,
        return sio.getvalue()

def get_object_list(path, Class):
    xml_string = GET(SERVER+path, credentials=CREDENTIALS)
    #print xml_string

    object_raw_list = minidom.parseString(xml_string)

    objects = []
    for object_element in object_raw_list.getElementsByTagName('resource'):
        object_dict = {}
        for child in object_element.childNodes:
            #print dir(child)
            object_dict[child.tagName] = child.firstChild.nodeValue

        xobject = Class.__new__(Class)
        xobject.__init__()
        xobject.load_from_dict(object_dict)
        objects.append(xobject)

    return objects

def get_servers():
    return get_object_list('server', Server)

def get_cameras():
    return get_object_list('camera', Camera)

def add_server(name, typeid):
    return POST(SERVER + 'server/', params = {'name' : name, 'xtype_id' : typeid}, credentials=CREDENTIALS, async=False)

def register_server():
    typeid = get_resource_types()[0].xid
    return PUT(SERVER + 'server/', params = {'id' : '1', 'name' : 'PEN ' + socket.gethostname()}, credentials=CREDENTIALS, async=False)

def get_resource_types():
    return get_object_list('resourceType', ResourceType)

def register_and_get():
    register_server()
    pprint(get_servers())

COMMANDS = {
  'add' : add_server,
  'reg' : register_server,
  'ls'  : get_servers,
  'lrt' : get_resource_types
}

def main():
    while 1:
        print '>> ',
        command = raw_input().split()
        if not command:
            continue
        
        if command[0] in ('help', '?'):
            print 'Commands: help, register, exit'

        if command[0] == 'exit':
            break

        if command[0] in COMMANDS:
            result = COMMANDS[command[0]]()
            pprint(result)

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] in COMMANDS:
        result = COMMANDS[sys.argv[1]]()
        pprint(result)
    else:
        main()
