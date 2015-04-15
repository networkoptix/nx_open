#!/usr/bin/python
'''nx_statistics -- NetworkOptix usage statistics server
'''

import json

from flask import Flask, request
from adapters import SqlAdapter
from storages import FileStorage

app = Flask(__name__)

def getAdapter():
    if not hasattr(app, 'sqlConnector'):
        import MySQLdb
        app.sqlConnector = MySQLdb # mysql by default
    app.SqlError = app.sqlConnector.Error

    if not hasattr(app, 'sqlConnection'):
        raise app.SqlError(0, 'Database is not configured')
    db = app.sqlConnector.connect(**app.sqlConnection)
    return SqlAdapter(db, app.logger)

def getStorage():
    if not hasattr(app, 'storage'):
        from os.path import expanduser
        app.storage = {'path': expanduser("~"), 'limit': 0}
    return FileStorage(log=app.logger, **app.storage)

# --- INTERFACE ---

@app.route('/api/reportStatistics', methods=['POST'])
def reportStatistics():
    try:
        getAdapter().report(request.get_json(True))
        return 'Success', 201
    except (KeyError,TypeError), e:
        if app.debug: raise
        return 'Request error: %s' % e.message, 400

@app.route('/api/sqlQuery/<string:query>', methods=['GET'])
def sqlQuery(query):
    try:
        return json.dumps(getAdapter().sqlQuery(query)), 200
    except app.SqlError as e:
        return 'SQL error: %s' % e.args[1], 400

# TODO: add different formats
@app.route('/api/sqlFormat/<string:query>', methods=['GET'])
def sqlFormat(query):
    try:
        return getAdapter().sqlFormat(query), 200
    except app.SqlError as e:
        return 'SQL error: %s' % e.args[1], 400

@app.route('/api/sqlChart/<string:query>', methods=['GET'])
def sqlChart(query):
    try:
        return getAdapter().sqlChart(query, **request.args).render_response()
    except (TypeError, ValueError) as e:
        if app.debug: raise
        return 'Request error: %s' % e.message, 400
    except app.SqlError as e:
        return 'SQL error: %s' % e.args[1], 400

@app.route('/api/deleteAll', methods=['GET'])
def deleteAll():
    if not app.debug:
        return 'deleteAll is avaliable only in debug mode', 404
    getAdapter().deleteAll()
    return 'Success', 201

@app.route('/api/reportCrash', methods=['POST'])
def reportCrash():
    try:
        args = {name.split('-')[1].lower(): value
            for name, value in request.headers.items() if name.startswith('Nx')}
        getStorage().write(request.get_data(), **args)
        return 'Success', 201
    except (IndexError, OSError), e:
        if app.debug: raise
        return 'Request error: wrong header data %s: %s' % (str(args), e.message)

@app.route('/api/listCrashes', methods=['GET'])
def listCrashes():
    try:
        args = {name: value[0] for name, value in request.args.items()}
        return json.dumps(getStorage().list(**args)), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (str(args), e.message)

@app.route('/api/getCrash', methods=['GET'])
def getCrash():
    try:
        args = {name: value for name, value in request.args.items()}
        return getStorage().read(**args), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (str(args), e.message)

@app.route('/api/deleteCrashes', methods=['GET'])
def deleteCrashes():
    try:
        args = {name: value for name, value in request.args.items()}
        return json.dumps(getStorage().delete(**args)), 200
    except (IndexError, OSError):
        if app.debug: raise
        return 'Request error: wrong params %s: %s' % (str(args), e.message)
