#!/usr/bin/python
'''CGI service for nx_statistics.crashserver
'''

from nx_statistics.crashserver import app

app.storage = dict(
    path = '/tmp/crashserver',
    limit = 1 * 1024 * 1024 * 1024, # 1gb
)

app.sendmail = dict(
    sender = 'crashserver@test.test',
    reciever = 'developer@test.test',
    subject = 'Crash Report',
)

app.download = 'http://localhost:8002/api/get?path='

appRun = dict(
    host = '127.0.0.1',
    port = 8001,
    debug = True,
    use_reloader = False,
)

if __name__ == '__main__':
    app.run(**appRun)
