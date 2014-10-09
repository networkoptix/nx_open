from bottle import route, get, post, run, template, request, static_file

settings = {
    "systemName": "My System",
    "port": 1234
}

@route('/hello/<name>')
def index(name):
    return template('<b>Hello {{name}}</b>!', name=name)

@get('/ec2/moduleInformation')
def get_settings():
    return '''{
  "reply": {
    "version": "2.3.0.0",
    "type": "Media Server",
    "systemName": "poe123",
    "systemInformation": "linux x64",
    "id": "{59f2e1b2-3259-43d7-b276-4e1c69d48c68}",
    "customization": "default"
  },
  "errorString": "",
  "error": 0
}'''

@post('/ec2/settings')
def save_settings():
    print request.json
    settings['systemName'] = request.json['systemName']
    settings['port'] = request.json['port']
    return {'status': 'OK'}
    
@post('/ec2/shutdownServer')
def shutdown_server():
    return {'status': 'OK'}

@get('/ec2/getHelp')
def get_help():
    return static_file('getHelp.xml', root='.', mimetype='text/xml')

@get('/ec2/getHelp.xsl')
def get_help():
    return static_file('getHelp.xsl', root='../app', mimetype='application/xslt+xml')

@post('/ec2/login')
def do_login():
    print 'Auth:', request.json
    return {'status': 'OK'}
    
run(host='localhost', port=9001, debug=True)
