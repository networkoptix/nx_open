#!/usr/bin/python
'''CGI service for nx_statistics
'''

from nx_statistics import app

import MySQLdb # or any other compartable
app.sqlConnector = MySQLdb

app.sqlConnection = dict(
    host = '127.0.0.1',
    user = 'root',
    passwd = '1',
    db = 'nx_statistics',
)

app.storage = dict(
    path = '/tmp/statserver_crashes',
    limit = 1 * 1024 * 1024 * 1024, # 1gb
)

appRun = dict(
    host = '127.0.0.1',
    port = 8000,
    debug = True,
    use_reloader = False,
)

if __name__ == '__main__':
    app.run(**appRun)
