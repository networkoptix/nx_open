#!/usr/bin/python
'''CGI service for nx_statistics.statserver
'''

from nx_statistics.statserver import app

import MySQLdb # or any other compartable
app.sqlConnector = MySQLdb

app.sqlConnection = dict(
    host = '127.0.0.1',
    user = 'root',
    passwd = '1',
    db = 'nx_statistics',
)

#app.geoipDb = dict(
#    path = '/home/mu/Downloads/GeoLite2-Country.mmdb',
#    service = 'country',
#)

appRun = dict(
    host = '127.0.0.1',
    port = 8002,
    debug = True,
    use_reloader = False,
)

if __name__ == '__main__':
    app.run(**appRun)
