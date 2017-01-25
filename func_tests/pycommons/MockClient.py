# $Id$
# Artem V. Nikitin
# Client emulator

import urllib2, FuncTest, json, base64, httplib, time
from Logger import LOGLEVEL
from Config import config
from ComparisonMixin import ComparisonMixin

DEFAULT_TIMEOUT =  10.0
DEFAULT_USER = 'admin'
DEFAULT_PASSWORD = 'admin'

HTTP_OK = 200

def params2url(params):
    def param2str(r):
        name, val = r
        return "%s=%s" % (name, val)
    return "&".join(map(param2str, params.items()))

class ServerResponse:
    
    def __init__(self, status, error = None, headers = {}):
        self.status = status
        self.headers = headers
        self.error = error

class Client:

    currentIdx = 0

    class ServerResponseData(ServerResponse):

        def __init__(self, request, response):
            ServerResponse.__init__(
                self, response.getcode(), headers = response.info())
            self.request = request
            self.response = response
            self.rawData = self.response.read()
            self.data = self.get_json()

        def get_json(self):
            if self.status == 200:
                try:
                    return json.loads(self.rawData)
                except ValueError:
                    return self.rawData
            return self.rawData

    def __init__(self, timeout = None):
        self._timeout = timeout or DEFAULT_TIMEOUT
        Client.currentIdx += 1
        self.index = Client.currentIdx

    def _processRequest(self, request):
        return urllib2.urlopen(request, timeout = self._timeout)

    def httpRequest(
        self, address, method, data = None, headers={},  **kw):
        url = "http://%s/%s" % (address, method)
        params = params2url(kw)
        if params:
            url+= '?' + params
        if data:
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "Client#%d POST request '%s':\n'%s'" % (self.index, url, data))
        else:
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "Client#%d GET request '%s'" % (self.index, url))
        try:
            hdrs = headers.copy()
            hdrs.setdefault('Connection', 'keep-alive')
            request = urllib2.Request(url, data=data, headers=hdrs)
            response = Client.ServerResponseData(
              url, self._processRequest(request))
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "Client#%d HTTP response:\n'%s'" % (self.index, response.data))
            return response
        except urllib2.HTTPError, x:
            FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d GET HTTP error '%s'" % (self.index, str(x)))
            return ServerResponse(x.code, x.reason, x.hdrs)
        except urllib2.URLError, x:
            FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d GET URL error '%s'" % (self.index, str(x)))
            return ServerResponse(None, x.reason, {})

class BasicAuthClient(Client):

    def __init__(self, user = None, password = None, timeout = None):
        Client.__init__(self, timeout)
        self.__user = user or config.get_safe("General", "username", DEFAULT_USER)
        self.__password = password or config.get_safe("General", "password", DEFAULT_PASSWORD)

    def _processRequest(self, request):
        base64string = \
          base64.encodestring('%s:%s' % (self.__user, self.__password)).replace('\n', '')
        request.add_header("Authorization", "Basic %s" % base64string)
        return urllib2.urlopen(request, timeout = self._timeout)

    @property
    def user(self):
        return self.__user

    @property
    def password(self):
        return self.__password


class DigestAuthClient(BasicAuthClient):

    RETRY_COUNT=3

    def __init__(self, user = None, password = None, timeout = None):
        BasicAuthClient.__init__(self, user, password, timeout)
        self.__handler = None
        self.__urlOpener = None

    def _update(self):
        passwordMgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
        self.__handler = urllib2.HTTPDigestAuthHandler(passwordMgr)
        self.__urlOpener = urllib2.build_opener(self.__handler)

    def _processRequest(self, request):
        if not self.__handler or not self.__urlOpener:
            self._update()
        self.__handler.add_password(
          None, request.get_full_url(), self.user, self.password)
        for i in range(self.RETRY_COUNT):
            try:
                response = self.__urlOpener.open(request, timeout = self._timeout)
                return response
            except urllib2.HTTPError, x:
                if x.code == 401:
                    self._update()
                raise
            except httplib.BadStatusLine, x:
                # From httplib.py:
                #   Presumably, the server closed the connection before
                #   sending a valid response.
                # It looks like an httplib issue, we may ignore it and try again.
                if i == self.RETRY_COUNT - 1:
                    raise
                FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d got unexpected HTTP status BadStatusLine %s" % (self.index, str(x)))


class ClientMixin(ComparisonMixin):

    def setUp(self):
        self.client = DigestAuthClient()

    def tearDown(self):
        pass

    # Check API call error
    def checkResponseError(self, response, method,
                           status = HTTP_OK,
                           apiErrorCode = 0,
                           apiErrorString = ''):
        self.assertEqual(response.status, status, "'%s' status" % method)
        if isinstance(response, Client.ServerResponseData) and status == HTTP_OK:
            self.assertFalse(type(response.data) is str, 'JSON response expected')
            if not isinstance(response.data, list):
                self.assertEqual(int(response.data.get('error', 0)),
                                 apiErrorCode, "'%s' reply.error != '%s'" % \
                                 (method,  apiErrorCode))
                self.assertEqual(
                    response.data.get('errorString', ''),
                    apiErrorString, "'%s' reply.errorString != '%s'" % \
                    (method, apiErrorString))

    # Call API method & check error
    def sendAndCheckRequest(self,
                            address,  method,
                            data = None,
                            headers={},
                            status = HTTP_OK,
                            **kw):
        if data:
            headers['Content-Type'] = 'application/json'
        response = self.client.httpRequest(
            address, method, data, headers, **kw)
        self.checkResponseError(response, method, status)
        return response
