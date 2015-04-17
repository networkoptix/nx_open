#!/usr/bin/python
'''WSGI service for nx_statistics.crashserver
'''

from nx_statistics.crashserver import app
from nx_statistics.log import handler

import json
import sys

app.storage = json.load(open(sys.argv[1]))
app.logger.addHandler(handler)

