#!/usr/bin/python

import json

from flask import Flask, request
from flaskext.mysql import MySQL

from adapters import SqlAdapter

app = Flask(__name__)

app.config['MYSQL_DATABASE_HOST'] = 'localhost'
app.config['MYSQL_DATABASE_USER'] = 'root'
app.config['MYSQL_DATABASE_PASSWORD'] = 'wsad123'
app.config['MYSQL_DATABASE_DB'] = 'nx_statistics'

sql = MySQL()
sql.init_app(app)

@app.route('/api/reportStatistics', methods=['POST'])
def reportStatistics():
    adapter = SqlAdapter(sql)
    data = json.loads(request.data)
    return adapter.report(data)

@app.route('/api/sqlQuery/<string:query>', methods=['GET'])
def sqlQuery(query):
    adapter = SqlAdapter(sql)
    data = adapter.sqlQuery(query)
    return json.dumps(data)

@app.route('/api/removeAll', methods=['GET'])
def removeAll():
    if not add.debug:
        raise Error("removeAll avaliable only in debug mode")
    adapter = SqlAdapter(sql)
    return adapter.removeAll()

if __name__ == '__main__':
    app.run(debug=True, use_reloader=False)
