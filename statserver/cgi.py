#!/usr/bin/python

import json

from flask import Flask, request
from flaskext.mysql import MySQL, MySQLdb

from adapters import SqlAdapter

app = Flask(__name__)

app.config['MYSQL_DATABASE_HOST'] = 'localhost'
app.config['MYSQL_DATABASE_USER'] = 'root'
app.config['MYSQL_DATABASE_PASSWORD'] = 'wsad123'
app.config['MYSQL_DATABASE_DB'] = 'nx_statistics'

sql = MySQL()
sql.init_app(app)
SqlError = MySQLdb.Error

@app.route('/api/reportStatistics', methods=['POST'])
def reportStatistics():
    try:
        SqlAdapter(sql).report(request.get_json())
        return 'Success', 201
    except KeyError, e:
        return 'Missing data in report: %s' % e.message, 400
    except TypeError, e:
        return 'Wrong data format: %s' % e.message, 400

@app.route('/api/sqlQuery/<string:query>', methods=['GET'])
def sqlQuery(query):
    try:
        return json.dumps(SqlAdapter(sql).sqlQuery(query)), 200
    except SqlError as e:
        return 'SqlError: %s' % e.args[1], 400

@app.route('/api/sqlChart/<string:query>', methods=['GET'])
def sqlChart(query):
    try:
        x, y = request.args.get('x', None), request.args.get('y', None)
        return SqlAdapter(sql).sqlChart(query, x, y).render_response()
    except SqlError as e:
        return 'SqlError: %s' % e.args[1], 400

@app.route('/api/removeAll', methods=['GET'])
def removeAll():
    if not add.debug:
        return 'removeAll is avaliable only in debug mode', 404
    SqlAdapter(sql).removeAll()
    return 'Success', 201

if __name__ == '__main__':
    app.run(debug=True, use_reloader=False)
