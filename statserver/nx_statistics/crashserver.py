#!/usr/bin/python
'''nx_statistics.crashserver -- NetworkOptix crash dumps server
'''

from storages import FileStorage
from flask import Flask, request

import json

app = Flask(__name__)

def getStorage():
    if not hasattr(app, 'storage'):
        from os.path import expanduser
        app.storage = {'path': expanduser("~"), 'limit': 0}
    return FileStorage(log=app.logger, **app.storage)

@app.route('/api/report', methods=['POST'])
def report_():
    try:
        args = {name.split('-')[1].lower(): value
            for name, value in request.headers.items() if name.startswith('Nx-')}
        getStorage().write(request.get_data(), **args)
        return 'Success', 201
    except (IndexError, OSError), e:
        if app.debug: raise
        return 'Request error: wrong header data %s: %s' % (str(args), e.message)

@app.route('/api/list', methods=['GET'])
def list_():
    try:
        args = {name: value[0] for name, value in request.args.items()}
        return json.dumps(getStorage().list(**args)), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (str(args), e.message)

@app.route('/api/get', methods=['GET'])
def get_():
    try:
        args = {name: value for name, value in request.args.items()}
        return getStorage().read(**args), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (str(args), e.message)

@app.route('/api/delete', methods=['GET'])
def delete_():
    try:
        args = {name: value for name, value in request.args.items()}
        return json.dumps(getStorage().delete(**args)), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (str(args), e.message)

