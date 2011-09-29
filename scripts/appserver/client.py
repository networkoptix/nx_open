#!/usr/bin/env python
# -*- coding: utf-8 -*-

from restclient import *
import json
from StringIO import StringIO
from pprint import pprint

SERVER="http://localhost:8000/api/"

class Object(object):
    def load_from_dict(self, xdict):
        for xstr, attr in self.attr_mapping.items():
            if xstr in xdict:
                self.__setattr__(attr, xdict[xstr])

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
    object_raw_list = json.loads(GET(SERVER+path))

    objects = []
    for object_dict in object_raw_list:
        xobject = Class.__new__(Class)
        xobject.__init__()
        xobject.load_from_dict(object_dict)
        objects.append(xobject)

    return objects

def get_servers():
    return get_object_list('server', Server)

def get_cameras():
    return get_object_list('camera', Camera)

def main():
    while 1:
        print '>> ',
        command = raw_input().split()
        if not command:
            continue
        
        if command[0] in ('help', '?'):
            print 'Commands: help, camera, server, exit'

        if command[0] == 'exit':
            break
        if command[0] == 'camera':
            pprint(get_cameras())
        elif command[0] == 'server':
            pprint(get_servers())
    

if __name__ == '__main__':
    main()
