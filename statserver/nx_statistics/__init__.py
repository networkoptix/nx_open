#!/usr/bin/python
'''nx_statistics -- NetworkOptix usage statistics server
'''

import json

from flask import Flask, request
from adapters import SqlAdapter

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

@app.route('/api/removeAll', methods=['GET'])
def removeAll():
    if not add.debug:
        return 'removeAll is avaliable only in debug mode', 404
    getAdapter().removeAll()
    return 'Success', 201

