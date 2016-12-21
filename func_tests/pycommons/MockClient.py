# $Id$
# Artem V. Nikitin
# Client emulator

import urllib2, FuncTest, json, base64, httplib
from Logger import LOGLEVEL
from Config import config
from ComparisonMixin import ComparisonMixin

DEFAULT_TIMEOUT =  10.0
DEFAULT_USER = 'admin'
DEFAULT_PASSWORD = 'admin'

class Client:

    currentIdx = 0

    class ServerResponse:

        def __init__(self, status, error = None):
            self.status = status
            self.error = error

    class ServerResponseData(ServerResponse):

        def __init__(self, request, response):
            Client.ServerResponse.__init__(self, response.getcode())
            self.request = request
            self.response = response
            self.data = self.get_json()

        def get_json(self):
            if self.status == 200:
                data = self.response.read()
                try:
                    return json.loads(data)
                except ValueError:
                    return data
            return None

    def __init__(self, user = None, password = None, timeout = None):
        self.__user = user or config.get_safe("General", "username", DEFAULT_USER)
        self.__password = password or config.get_safe("General", "password", DEFAULT_PASSWORD)
        self._timeout = timeout or DEFAULT_TIMEOUT
        Client.currentIdx += 1
        self.index = Client.currentIdx

    @property
    def user(self):
        return self.__user

    @property
    def password(self):
        return self.__password

    def _params2url(self, params):
        def param2str(r):
            name, val = r
            return "%s=%s" % (name, val)
        return "&".join(map(param2str, params.items()))

    def _processRequest(self, request, user, password):
        base64string = \
          base64.encodestring('%s:%s' % (user, password)).replace('\n', '')
        request.add_header("Authorization", "Basic %s" % base64string)
        return urllib2.urlopen(request, timeout = self._timeout)

    def httpRequest(
        self, address, method,
        data = None,
        headers={},
        auth_user = None,
        auth_password = None,  **kw):
        user = auth_user or self.__user
        password = auth_password or self.__password
        url = "http://%s/%s" % (address, method)
        params = self._params2url(kw)
        if params:
            url+= '?' + params
        if data:
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "Client#%d POST request '%s':\n'%s'" % (self.index, url, data))
        else:
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "Client#%d GET request '%s'" % (self.index, url))
        try:
            request = urllib2.Request(url, data=data, headers=headers)
            response = Client.ServerResponseData(
              url, self._processRequest(request, user, password))
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "Client#%d HTTP response:\n'%s'" % (self.index, response.data))
            return response
        except urllib2.HTTPError, x:
            FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d GET HTTP error '%s'" % (self.index, str(x)))
            return Client.ServerResponse(x.code, x.reason)
        except urllib2.URLError, x:
            FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d GET URL error '%s'" % (self.index, str(x)))
            return Client.ServerResponse(None, x.reason)

class DigestAuthClient(Client):

    RETRY_COUNT=3

    def __init__(self, user = None, password = None, timeout = None):
        Client.__init__(self, user, password, timeout)
        self.__handler = None
        self.__urlOpener = None

    def _update(self):
        passwordMgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
        self.__handler = urllib2.HTTPDigestAuthHandler(passwordMgr)
        self.__urlOpener = urllib2.build_opener(self.__handler)

    def _processRequest(self, request, user, password):
        if not self.__handler or not self.__urlOpener:
            self._update()
        self.__handler.add_password(
          None, request.get_full_url(), user, password)
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
    def checkResponseError(self, response, method):
        self.assertEqual(response.status, 200, "'%s' status" % method)
        if isinstance(response, Client.ServerResponseData):
            self.assertFalse(type(response.data) is str, 'JSON response expected')
            if not isinstance(response.data, list):
                self.assertEqual(int(response.data.get('error', 0)), 0, "'%s' reply.error" % method)
                self.assertEqual(response.data.get('errorString', ''), '', "'%s' reply.errorString" % method)

    # Call API method & check error
    def sendAndCheckRequest(self,
                            address,  method,
                            data = None,
                            headers={},
                            auth_user = None,
                            auth_password = None,  **kw):
        response = self.client.httpRequest(
            address, method, data, headers,
            auth_user, auth_password, **kw)
        self.checkResponseError(response, method)
        return response

                
