#!/usr/bin/python
'''nx_statistics.crashserver -- NetworkOptix crash dumps server
'''

from storages import FileStorage
from utils import sendmail
from flask import Flask, request

import json

app = Flask(__name__)

def getStorage():
    if not hasattr(app, 'storage'):
        from os.path import expanduser
        app.storage = dict(path=expanduser("~"), limit=0)
    return FileStorage(log=app.logger, **app.storage)

def mailNotify(info):
    addr = getattr(app, 'download', None)
    mail = getattr(app, 'sendmail', None)
    if not addr or not mail: return
    text = 'Crash: %s\nDownload: %s' % (json.dumps(info), addr+info['path'])
    sendmail(text=text, **mail)

@app.route('/api/report', methods=['POST'])
def report():
    try:
        headers = {n: v for n, v in request.headers.items()
                   if n.startswith('Nx-')}
        args = {n.split('-')[1].lower(): v for n, v in headers.items()}
        mailNotify(getStorage().write(request.get_data(), **args))
        return 'Success', 201
    except (IndexError, OSError), e:
        if app.debug: raise
        return 'Request error: wrong header data %s: %s' % (
            str(headers), e.message)

def getStorageTemplate(method, wrap=json.dumps):
    try:
        args = {name: value[0] for name, value in request.args.items()}
        print args
        call = getattr(getStorage(), method)
        return wrap(call(**args)), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (
            str(request.args), e.message)

@app.route('/api/list', methods=['GET'])
def list_(): return getStorageTemplate('list')

@app.route('/api/last', methods=['GET'])
def last(): return getStorageTemplate('last')

@app.route('/api/get', methods=['GET'])
def get(): return getStorageTemplate('read', wrap=lambda x: x)

@app.route('/api/delete', methods=['GET'])
def delete(): return getStorageTemplate('delete')

