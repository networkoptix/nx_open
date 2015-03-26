#!/usr/bin/python

import os
import json

from datetime import datetime
from flask import Flask, request


API_STAT = '/api/reportStatistics'
OUTPUT = '/tmp/statserver_debug_cgi'

app = Flask(__name__)

@app.route('/api/reportStatistics', methods=['POST'])
def reportMediaserver():
    data = json.dumps(json.loads(request.data), indent=4, sort_keys=True)
    with open("%s/%s" % (OUTPUT, datetime.now()), "w") as f:
        f.write(data)
    return json.dumps({'success':True}), 200, {'ContentType':'application/json'}

if __name__ == '__main__':
    if not os.path.exists(OUTPUT): os.makedirs(OUTPUT)
    print 'Save reports from %s to %s' % (API_STAT, OUTPUT)
    app.run(debug=True, use_reloader=False)
