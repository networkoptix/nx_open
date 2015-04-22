#!/usr/bin/python
'''WSGI service for nx_statistics.crashserver
'''

from nx_statistics.crashserver import app
from nx_statistics.utils import logHandler

import json
import sys

config = sys.argv[1]
for option, value in json.load(open(config)).items():
    setattr(app, option, value)

app.logger.addHandler(logHandler)

