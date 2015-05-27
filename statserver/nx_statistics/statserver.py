#!/usr/bin/python
'''nx_statistics.statserver -- NetworkOptix usage statistics server
'''

from flask import Flask, request
from adapters import SqlAdapter

import json, os

app = Flask(__name__)

def getAdapter():
    if not hasattr(app, 'sqlConnector'):
        import MySQLdb
        app.sqlConnector = MySQLdb # mysql by default
    app.SqlError = app.sqlConnector.Error

    if not hasattr(app, 'sqlConnection'):
        raise app.SqlError(0, 'Database is not configured')
    db = app.sqlConnector.connect(**app.sqlConnection)

    ipResolve = None
    if hasattr(app, 'geoipDb') and os.path.isfile(app.geoipDb):
        from geoip2.database import Reader as IpReader
        reader = IpReader(app.geoipDb)
        cache = {}
        def resolve(ip):
            if ip not in cache:
                cache[ip] = reader.country(ip).country.iso_code
            print cache[ip]
            return cache[ip]
        ipResolve = resolve

    return SqlAdapter(db, app.logger, ipResolve)

@app.route('/api/report', methods=['POST'])
def report():
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

