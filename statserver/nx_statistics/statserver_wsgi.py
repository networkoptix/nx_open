#!/usr/bin/python
'''WSGI service for nx_statistics.statserver
'''

from nx_statistics.statserver import app
from nx_statistics.log import handler

import sys
import json
import MySQLdb

app.sqlConnector = MySQLdb
app.sqlConnection = json.load(open(sys.argv[1]))
app.logger.addHandler(handler)

