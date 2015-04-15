#!/usr/bin/python
'''WSGI service for nx_statistics
'''

from nx_statistics import app

import sys
import json

import MySQLdb # or any other compartable
app.sqlConnector = MySQLdb

config = json.load(open(sys.argv[1])
app.sqlConnection = config['sql']
app.storage = config['storage']

import logging
formatter = logging.Formatter(
    "[%(asctime)s] {%(pathname)s:%(lineno)d} %(levelname)s - %(message)s")
handler = logging.StreamHandler()
handler.setLevel(logging.DEBUG) # TODO: consider to change to INFO
handler.setFormatter(formatter)
app.logger.addHandler(handler)
