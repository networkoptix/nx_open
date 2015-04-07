#!/usr/bin/python
'''WSGI service for nx_statistics
'''

from nx_statistics import app

import sys
import json
import MySQLdb # or any other compartable
app.sqlConnector = MySQLdb

config_file = sys.argv[1]
app.sqlConnection = json.load(open(config_file))
