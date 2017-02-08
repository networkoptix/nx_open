import json
import requests


REST_API_USER = 'admin'
REST_API_PASSWORD = 'admin'
REST_API_TIMEOUT_SEC = 3


class ServerRestApiProxy(object):

    def __init__(self, url):
        self._url = url

    def __getattr__(self, name):
        return ServerRestApiProxy(self._url + '/' + name)

    def get(self, raise_exception=True, **kw):
        print 'GET', self._url, kw
        return self._response2json(
            requests.get(self._url, params=kw, auth=(REST_API_USER, REST_API_PASSWORD), timeout=REST_API_TIMEOUT_SEC),
            raise_exception)

    def post(self, raise_exception=True, **kw):
        print 'POST', self._url, kw
        return self._response2json(
            requests.post(self._url, json=kw, auth=(REST_API_USER, REST_API_PASSWORD), timeout=REST_API_TIMEOUT_SEC),
            raise_exception)

    def _response2json(self, response, raise_exception):
        print '\t--> [%d] %s' % (response.status_code, response.content)
        if raise_exception:
            response.raise_for_status()
        json = response.json()
        if type(json) != type({}) or not raise_exception:
            return json
        error = json.get('error')
        if error is None:
            return json
        if int(error):
            print json
            raise RuntimeError('Server REST request returned error: %s' % json['errorString'])
        else:
            return json['reply']


class ServerRestApi(object):

    def __init__(self, url):
        self.ec2 = ServerRestApiProxy(url + 'ec2')
        self.api = ServerRestApiProxy(url + 'api')
