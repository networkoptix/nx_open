# $Id$
# Artem V. Nikitin
# Client emulator

import urllib2, FuncTest, json, base64, httplib
from Logger import LOGLEVEL
from Config import config

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
        self, address,  command,
        data = None,
        headers={},
        auth_user = None,
        auth_password = None,  **kw):
        user = auth_user or self.__user
        password = auth_password or self.__password
        url = "http://%s/%s" % (address, command)
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
            FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d GET HTTP error '%s'" % (self.index, str(x)))
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
                if i == self.RETRY_COUNT - 1:
                    raise
                # Sometimes we've got unexcpected BadStatusLine with empty status line
                # It looks like an httplib bug, we may ignore it and try again
                FuncTest.tlog(LOGLEVEL.ERROR, "Client#%d got unexpected HTTP status BadStatusLine %s" % (self.index, str(x)))
                
