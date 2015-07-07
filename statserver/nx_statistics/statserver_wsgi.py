#!/usr/bin/python
'''WSGI service for nx_statistics.statserver
'''

from nx_statistics.statserver import app
from nx_statistics.utils import logHandler

import sys
import json
import MySQLdb

config = sys.argv[1]
for option, value in json.load(open(config)).items():
    setattr(app, option, value)

app.sqlConnector = MySQLdb
app.logger.addHandler(logHandler)

