# $Id$
# Artem V. Nikitin
# HLS mediaserver client

import urllib2, FuncTest, socket
from MockClient import DEFAULT_TIMEOUT, DEFAULT_USER, DEFAULT_PASSWORD, params2url, ServerResponse
from Config import config
from Logger import LOGLEVEL, logException

class HlsClient(object):

    class ResponseData(ServerResponse):

        def __init__(self, request, response):
            ServerResponse.__init__(
                self, response.getcode(), headers = response.info())
            self.request = request
            self.response = response
            self.data = self.response.read()

    class M3UResponseData(ResponseData):
        def __init__(self, request, response):
            HlsClient.ResponseData.__init__(
                self, request, response)
            self.m3uList = []
            self.__parseData()

        def __parseData(self):
            dataLines = map(lambda l: l.strip(), self.data.split("\n"))
            if dataLines[0] != "#EXTM3U":
                raise Exception("Invalid m3u: %s" % self.data)
            for l in dataLines[1:]:
                if l.startswith('/hls/'):
                    self.m3uList.append(l)

    def __init__(self, timeout = DEFAULT_TIMEOUT, user = None, password = None):
        self.__timeout = timeout
        self.__user = user or config.get_safe("General", "username", DEFAULT_USER)
        self.__password = password or config.get_safe("General", "password", DEFAULT_PASSWORD)
        self.__handler = None
        self.__urlOpener = None
        self.m3uList = []

    def __update(self):
        passwordMgr = urllib2.HTTPPasswordMgrWithDefaultRealm()
        self.__handler = urllib2.HTTPDigestAuthHandler(passwordMgr)
        self.__urlOpener = urllib2.build_opener(self.__handler)

    def __processRequest(self, request, responseCls = ResponseData):
        if not self.__handler or not self.__urlOpener:
            self.__update()
        url = request.get_full_url()
        FuncTest.tlog(LOGLEVEL.DEBUG + 9, "HLS request '%s'" % url)
        try:
            self.__handler.add_password(
                None, request.get_full_url(), self.__user, self.__password)
            response = responseCls(
              url, self.__urlOpener.open(request, timeout = self.__timeout))
            FuncTest.tlog(LOGLEVEL.DEBUG + 9, "HLS response: status: %d, length: %s  " % \
                          (response.status, response.headers.get('Content-Length', 0)))
            return response
        except urllib2.HTTPError, x:
            if x.code == 401:
                self.__update()
            FuncTest.tlog(LOGLEVEL.ERROR, "HLS error '%s'" % str(x))
            return ServerResponse(x.code, x.reason, x.hdrs)
        except urllib2.URLError, x:
            FuncTest.tlog(LOGLEVEL.ERROR, "HLS error '%s'" % str(x))
            return ServerResponse(None, x.reason, {})

    def requestM3U(self, address, srvGuid, camera, headers={},  **kw):
        hdrs = headers.copy()
        hdrs.setdefault('x-server-guid', srvGuid)
        url = "http://%s/hls/%s.m3u" % (address, camera.physicalId)
        params = params2url(kw)
        if params:
            url += "?" + params
        request = urllib2.Request(url, headers=hdrs)
        response = self.__processRequest(request, self.M3UResponseData)
        self.m3uList = response.m3uList
        return response        
                
            
    
        
       
        
