#!/usr/bin/env python

from restclient import *
from StringIO import StringIO
from pprint import pprint

from xml.dom import minidom
import socket, sys

SERVER = "http://localhost:8000/api/"
CREDENTIALS = ("ivan", "225653")

debug = False

RESOURCE_STATUS = (
    (1, 'Inactive'),
    (2, 'Active'),
)

SERVER_ID = None

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

class Resource(Object):
    def __init__(self):
        Object.__init__(self)

        self.attr_mapping = {'id' : 'xid', 'name' : 'name', 'status' : 'status', 'type' : 'xtype'}

        self.xid = ''
        self.name = ''
        self.status = 0
        self.xtype = ''

    def __repr__(self):
        sio = StringIO()
        print >> sio, 'ID: %s\t' % self.xid,
        print >> sio, ', Name: %s\t' % self.name,
        print >> sio, ', Status: %s\t' % self.status,
        print >> sio, ', Type: %s\t' % self.xtype,
        return sio.getvalue()

class Camera(Resource):
    def __init__(self):
        Resource.__init__(self)

        self.attr_mapping.update({'mac' : 'mac', 'url' : 'url', 'login' : 'login', 'password' : 'password'})

        self.mac = ''
        self.url = ''
        self.login = ''
        self.password = ''

    def __repr__(self):
        sio = StringIO()
        print >> sio, Resource.__repr__(self),
        print >> sio, ', Mac: %s\t' % self.mac,
        print >> sio, ', Url: %s\t' % self.url,
        print >> sio, ', Login: %s\t' % self.login,
        print >> sio, ', Password: %s\t' % self.password,
        return sio.getvalue()
        
class Server(Resource):
    def __init__(self):
        Resource.__init__(self)

        self.attr_mapping.update({})

    def __repr__(self):
        sio = StringIO()
        print >> sio, Resource.__repr__(self),
        return sio.getvalue()

class Layout(Resource):
    def __init__(self):
        Resource.__init__(self)

        self.attr_mapping.update({'user_id' : 'user_id', 'background_url' : 'background_url'})

        self.user = ''
        self.background_url = ''

    def __repr__(self):
        sio = StringIO()
        print >> sio, Resource.__repr__(self),
        print >> sio, ', User: %s\t' % self.user,
        print >> sio, ', Background: %s\t' % self.background,
        return sio.getvalue()

def get_object_list(path, Class, args=''):
    request_str = SERVER + path
    if args:
        request_str += '?' + args

    xml_string = GET(request_str, credentials=CREDENTIALS)
    if debug:
        print xml_string

    object_raw_list = minidom.parseString(xml_string)

    objects = []
    resource_elements = object_raw_list.getElementsByTagName(path)
    for object_element in resource_elements:
        object_dict = {}
        for child in object_element.childNodes:
            if child.childNodes:
                object_dict[child.tagName] = child.firstChild.nodeValue

        xobject = Class.__new__(Class)
        xobject.__init__()
        xobject.load_from_dict(object_dict)
        objects.append(xobject)

    if not resource_elements:
        if object_raw_list.childNodes:
            if object_raw_list.childNodes[0].childNodes:
                object_dict = {}
                for child in object_raw_list.childNodes[0].childNodes:
                    if child.childNodes:
                        object_dict[child.tagName] = child.firstChild.nodeValue

                xobject = Class.__new__(Class)
                xobject.__init__()
                xobject.load_from_dict(object_dict)
                objects.append(xobject)

    return objects

# Commands

def list_cameras():
    return get_object_list('camera', Camera)

def add_server():
    typeid = list_resource_types()[0].xid
    return POST(SERVER + 'server/', params = {'name' : socket.gethostname(), 'xtype_id' : typeid}, credentials=CREDENTIALS, async=False)

def add_camera():
    serverid = list_servers()[0].xid
    typeid = list_resource_types()[0].xid
    return POST(SERVER + 'camera/', params = {'name' : "new camera", 'server_id' : serverid, 'xtype_id' : typeid}, credentials=CREDENTIALS, async=False)

def reg_server():
    return PUT(SERVER + 'server/', params = {'id' : SERVER_ID}, credentials=CREDENTIALS, async=False)

def reg_camera(*args):
    typeid = list_resource_types()[0].xid
    params = {'name' : socket.gethostname()}

    for arg in args:
        name, value = arg.split('=')
        params[name] = value

    return PUT(SERVER + 'camera/', params = params, credentials=CREDENTIALS, async=False)

def list_servers():
    return get_object_list('server', Server)

def list_resource_types():
    return get_object_list('resourceType', ResourceType)

def list_resources(*args):
    if args:
        return get_object_list('resource', Resource, args[0])
    else:
        return get_object_list('resource', Resource)

def list_resources_as_tree(*args):
    if args:
        return get_object_list('resource/tree', Resource, args[0])
    else:
        return get_object_list('resource/tree', Resource)

def add_layout(*args):
    typeid = list_resource_types()[0].xid
    return POST(SERVER + 'layout/', params = {'name' : "new layout", 'xtype_id' : typeid}, credentials=CREDENTIALS, async=False)

def list_layouts(*args):
    return get_object_list('layout', Layout)

COMMANDS = {
  'as'  : add_server,
  'ls'  : list_servers,
  'rs'  : reg_server,

  'ac'  : add_camera,
  'rc'  : reg_camera,
  'lc'  : list_cameras,

  'lr'  : list_resources,
  'lrat': list_resources_as_tree,
 
  'al'  : add_layout,
  'll'  : list_layouts,
  'lrt' : list_resource_types,
}

def help():
    print 'Commands: help, register, exit'

def main():
    while 1:
        print '>> ',
        command = raw_input().split()
        if not command:
            continue
        
        if command[0] in ('help', '?'):
            help()

        if command[0] == 'exit':
            break

        if command[0] in COMMANDS:
            result = COMMANDS[command[0]]()
            pprint(result)

def read_state():
    global SERVER_ID

    for line in open('server.state', 'r').readlines():
        name, value = line.split('=')
        if name == 'SERVER_ID':
            SERVER_ID = value.strip()

def scanario1():
    # register server
    reg_server()

    resource_types = list_resource_types()

    register_server()

if __name__ == '__main__':
    read_state()

    debug = True
    if len(sys.argv) == 2 and sys.argv[1] == 'test':
        scenario1()
        sys.exit(0)

    if len(sys.argv) > 1:
        if sys.argv[1] == '-d':
            debug = True
            command = sys.argv[2]

            args = ()
            if len(sys.argv) > 3:
                args = sys.argv[3:]
        else:
            debug = False
            command = sys.argv[1]

            args = ()
            if len(sys.argv) > 2:
                args = sys.argv[2:]

        if command in COMMANDS:
            if args:
                result = COMMANDS[command](*args)
            else:
                result = COMMANDS[command]()
            pprint(result)
        else:
            help()
    else:
        main()
