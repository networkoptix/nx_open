# -*- coding: utf-8 -*-
__author__ = 'Danil Lavrentyuk'
"""Test inter-server redirects."""

import sys
import urllib2
#from httplib import HTTPException
import json
import time
from collections import namedtuple
import traceback
from functest_util import JsonDiff, compareJson, get_server_guid, real_caps, textdiff
from testbase import FuncTestError
from pycommons.Logger import log, LOGLEVEL

_MAIN_HOST = '192.168.109.8:7001'
_SEC_HOST = '192.168.109.9:7001'
#FIXME USE CONFIG!!!!

#CHECK_URI = ''

_USER = 'admin'
_PWD = 'admin'


def _prepareLoader(hosts):
    try:
        passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
        for h in (hosts):
            passman.add_password(None, "http://%s/ec2" % h, _USER, _PWD)
            passman.add_password(None, "http://%s/api" % h, _USER, _PWD)
        urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))
    except Exception:
        raise FuncTestError("can't install a password manager: %s" % (traceback.format_exc(),))


Result = namedtuple('Result', ['func', 'server', 'redirect', 'rawdata', 'data', 'length'])
def _req_str(self):
    return (
        "direct request to %s" % self.server
    ) if self.redirect is None else (
        "request to %s through %s" % (self.redirect, self.server)
    )
Result.req_str = _req_str


def compareResults(a, b):
    """
    :param a: Result
    :param b: Result
    """
    if a.func != b.func:
        raise AssertionError("The test is broken! Trying to compare different functions results: %s and %s" %
                             (a.funcm, b.func))
    if a.length != b.length:
        log(LOGLEVEL.INFO, "Function %s. Different data lengths returned by %s (%s) and %s (%s)" %
                            (a.func, a.req_str(), a.length, b.req_str(), b.length))
        # dont raise because we want to show diff
    diff = compareJson(a.data, b.data)
    if diff.hasDiff():
        diffresult = textdiff(a.rawdata, b.rawdata, a.req_str(), b.req_str())
        raise FuncTestError("Function %s. Diferent responses for %s and %s:\n%s\nText diff:\n%s" %
                            (a.func, a.req_str(), b.req_str(), diff.errorInfo(), diffresult))


class ServerProxyTest(object):

    def __init__(self, mainHost, secHost):
        self.mainHost = mainHost
        self.secHost = secHost
        self.uids = {}

    def getUids(self):
        for h in (self.mainHost, self.secHost):
            guid = get_server_guid(h)
            if guid is not None:
                self.uids[h] = guid
                log(LOGLEVEL.DEBUG + 9, "%s - %s" % (h, guid))
            else:
                raise FuncTestError("Can't get server %s guid!" % h)

    def _performRequest(self, func, peer, redirectTo=None):  # :type : Result
        action = "requesting %s from %s" % (func, peer if not redirectTo else ("%s through %s" % (redirectTo, peer)))
        log(LOGLEVEL.INFO, real_caps(action))
        try:
            req = urllib2.Request('http://%s/%s' % (peer, func))
            if redirectTo:
                req.add_header('X-server-guid', self.uids[redirectTo])
            response = urllib2.urlopen(req)
            data = response.read()
            content_len = int(response.info()['Content-Length'])
            if content_len != len(data):
                raise FuncTestError(
                    "Wrong result %s: resulting data len: %s. Content-Length: %s" % (action, len(data), content_len))
            return Result(func, peer, redirectTo, data, json.loads(data), content_len)
        except urllib2.HTTPError as err:
#        except HTTPException as err:
            log(LOGLEVEL.ERROR, "FAIL: %s raised %s" % (action, err))
            raise FuncTestError("error " + action, str(err))

    def run(self):
        log(LOGLEVEL.INFO, "\n=======================================")
        log(LOGLEVEL.INFO, "Proxy Test Start")
        try:
            self.getUids()
            func = 'ec2/getFullInfo?extraFormatting'
            time.sleep(0.1)
            res1 = self._performRequest(func, self.mainHost)
            time.sleep(0.1)
            res2 = self._performRequest(func, self.secHost)
            compareResults(res1, res2)
            time.sleep(0.1)
            res2a = self._performRequest(func, self.mainHost, self.secHost)
            compareResults(res2, res2a)
            time.sleep(0.1)
            func = 'api/moduleInformation'
            res1 = self._performRequest(func, self.secHost)
            time.sleep(0.1)
            res2 = self._performRequest(func, self.mainHost, self.secHost)
            compareResults(res1, res2)
            res1 = self._performRequest(func, self.mainHost)
            time.sleep(0.1)
            res2 = self._performRequest(func, self.secHost, self.mainHost)
            compareResults(res1, res2)
            #time.sleep(1)
            #res2 = self._performRequest(func, self.mainHost)
            #if res1.length == res2.length:
            #    diff = compareJson(res1.data, res2.data)
            #    if not diff.hasDiff():
            #        raise FuncTestError("")
            log(LOGLEVEL.INFO, "Test complete. Responses are the same.")
            return True
        except FuncTestError:
            raise
        except Exception:
            raise FuncTestError(traceback.format_exc())
        finally:
            log(LOGLEVEL.INFO, "=======================================")


if __name__ == '__main__':
    try:
        _MAIN_HOST = sys.argv[1]
        _SEC_HOST = sys.argv[2]
    except IndexError:
        pass
    try:
        _prepareLoader((_MAIN_HOST, _SEC_HOST))
        ServerProxyTest(_MAIN_HOST, _SEC_HOST).run()
    except FuncTestError as err:
        log(LOGLEVEL.ERROR, "FAIL: %s" % err.message)

