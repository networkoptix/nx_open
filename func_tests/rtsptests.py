# -*- coding: utf-8 -*-
""" All RTSP tests and their additional classes, called from functest.py
Initially moved out from functest.py
+ Also performs HTTP tstreaming test (using webm format)
"""
__author__ = 'Danil Lavrentyuk'
import errno, json, random, re, select, signal, socket, sys, time, traceback
import base64
from hashlib import md5
import pprint
import urllib2, httplib
import threading
from collections import namedtuple
from itertools import imap
from pycommons.Logger import log, LOGLEVEL, logException, initLog

from functest_util import SafeJsonLoads, HttpRequest, parse_size, quote_guid, \
    CAMERA_ATTR_EMPTY, CAMERA_ID_FIELD, FULL_SCHEDULE_TASKS

#import pprint

# RTSP performance test constants
_DEFAULT_ARCHIVE_STREAM_RATE = 1024 * 1024 * 10 # 10 MB/Sec
_DEFAULT_CAMERAS_START_GRACE = 5  # seconds
_DEFAULT_THREAD_START_GAP = 0.001 # seconds
_DEFAULT_SOCKET_CLOSE_GRACE = 0.01 # seconds
_DEFAULT_LIVE_DATA_PART = 50  # percents, whole number
# for streaming tests:
_DEFAULT_TEST_THREADS = 10
_DEFAULT_STREAMING_DURATION = 90


_urlsub = re.compile(r'[:/?]')
_urlsubtbl = {':': '$', '/': '%', '?': '#'}

def _urlsubfunc(m):
    return _urlsubtbl[m.group(0)]

def _buildUrlPath(url):
    return _urlsub.sub(_urlsubfunc, url) + '_' + ''.join(str(random.randint(0,9)) for _ in xrange(12))

def RandomArchTime(_min, _max):
    "Get random time from _max to _min minutes in the past. Returns as number of microseconds."
    now = time.time()
    pos = now - random.randint(60 * _min, 60 * _max)
    log(LOGLEVEL.INFO, "Generated time position %s, from now by %s m %s s" % (int(pos), int(pos - now)/60, int(pos - now)%60))
    return int(pos * 1e6)
    #return int((time.time() - random.randint(60 * _min, 60 * _max)) * 1e6)

HTTP_STREAM_FORMAT = 'webm'

class StreamURLBase(object):
    _streamURLTemplate = ''
    _server = None
    _port = None
    _mac = None

    def __init__(self, (server, port), mac):
        self._server = server
        self._port = port
        self._mac = mac

    def generateURL(self):
        return self._streamURLTemplate % (self._server, self._port, self._mac)


class RtspStreamURLGenerator(StreamURLBase):
    _streamURLTemplate = "rtsp://%s:%s/%s"


class HttpStreamURLGenerator(StreamURLBase):
    _streamURLTemplate = "http://%s:%s/media/%s." + HTTP_STREAM_FORMAT + "?sfd=1"


class ArchiveURLBase(object):
    _archiveURLTemplate = ''
    _diffMax = 5
    _diffMin = 1
    _server = None
    _port = None
    _mac = None

    def __init__(self, max, min, (server, port), mac):
        self._diffMax = max
        self._diffMin = min
        self._server = server
        self._port = port
        self._mac = mac

    def generateURL(self):
        return self._archiveURLTemplate % (self._server, self._port, self._mac, RandomArchTime(self._diffMin, self._diffMax))


class RtspArchiveURLGenerator(ArchiveURLBase):
    _archiveURLTemplate = "rtsp://%s:%s/%s?pos=%d"


class HttpArchiveURLGenerator(ArchiveURLBase):
    _archiveURLTemplate = "http://%s:%s/media/%s." + HTTP_STREAM_FORMAT + "?sfd=1&pos=%d"


# RTSP global backoff timer, this is used to solve too many connection to server
# which makes the server think it is suffering DOS attack
class StreamingBackOffTimer:
    _timerLock = threading.Lock()
    _globalTimerTable= dict()

    MAX_TIMEOUT = 4.0
    HALF_MAX_TIMEOUT = MAX_TIMEOUT / 2.0
    MIN_TIMEOUT = 0.01

    @classmethod
    def increase(cls, url):
        with cls._timerLock:
            if url in cls._globalTimerTable:
                if cls._globalTimerTable[url] > cls.HALF_MAX_TIMEOUT:
                    cls._globalTimerTable[url] = cls.MAX_TIMEOUT
                else:
                    cls._globalTimerTable[url] *= 2.0
            else:
                cls._globalTimerTable[url] = cls.MIN_TIMEOUT
        # it's not a problem if some other thread change cls._globalTimerTable[url] this moment -
        # let it sleep the newly corrected length,
        # but not lock other threads from access other cls._globalTimerTable elements while it sleeps
        time.sleep(cls._globalTimerTable[url])

    @classmethod
    def decrease(cls, url):
        with cls._timerLock:
            if url in cls._globalTimerTable:
                if cls._globalTimerTable[url] <= cls.MIN_TIMEOUT:
                    cls._globalTimerTable[url] = cls.MIN_TIMEOUT
                else:
                    cls._globalTimerTable[url] /= 2.0


class DummyLock(object):
    def __enter__(self):
        pass
    def __exit__(self,type,value,trace):
        pass


class StreamTcpBasic(object):
    _socket = None
    _resolution = None

    _rtspTemplate = "\r\n".join((
        "PLAY %(url)s RTSP/1.0",
        "CSeq: 2",
        "Range: npt=now-",
        "Scale: 1",
        "x-guid: %(cid)s",
        "Session:",
        "User-Agent: Network Optix",
        "x-play-now: true",
        "Authorization: %(auth)s",
        "x-server-guid: %(sid)s", '', '')) # two '' -- to add two '\r\n' at the end

    _httpTemplate = "\r\n".join((
        "GET %(url)s HTTP/1.0",
        "Authorization: %(auth)s",
        "x-server-guid: %(sid)s", '', ''))

    _digestAuthTemplate = 'Digest username="%s",realm="%s",nonce="%s",uri="%s",response="%s",algorithm="MD5"'

    _skip_errno = [errno.EAGAIN, errno.EWOULDBLOCK] + ([errno.WSAEWOULDBLOCK] if hasattr(errno, 'WSAEWOULDBLOCK') else [])
    # There is no errno.WSAEWOULDBLOCK on Linux.

    def __init__(self, proto, (addr, port), (mac, cid), sid, uname, pwd, urlGen, lock=None, socket_reraise=False):
        self._addr = addr
        self._port = int(port)
        self._urlGen = urlGen
        self._url = urlGen.generateURL()
        self._basic_auth = 'Basic ' + base64.encodestring('%s:%s' % (uname, pwd)).rstrip()
        self._proto = proto
        self._tpl = self._rtspTemplate if proto == 'rtsp' else self._httpTemplate
        self._params = {'url': self._url, 'cid': cid, 'auth': self._basic_auth, 'sid': sid}
        self._data = self._tpl % self._params

        self._cid = cid
        self._sid = sid
        self._mac = mac
        self._uname = uname
        self._pwd = pwd
        self._lock = lock if lock is not None else DummyLock()
        self._socket_reraise = socket_reraise

    def add_prefered_resolution(self, resolution):
        if self._proto == 'rtsp': # FIXME reactor it to support HTTP case and use URL parameter 'resolution'
            self._resolution = resolution
            self._data = self._add_resolution_str(self._data)

    def _add_resolution_str(self, header):
        return ''.join((header[:-2], 'x-media-quality: ', self._resolution, "\r\n\r\n"))

    def _logError(self, msg):
        with self._lock:
            log(LOGLEVEL.ERROR, msg)

    def _checkEOF(self,data):
        return data.find("\r\n\r\n") > 0

    def _parseRelamAndNonce(self, reply):
        idx = reply.find("WWW-Authenticate")
        if idx < 0:
            return None
        # realm is fixed for our server - NO! it's chhanged already!
        realm = "NetworkOptix" #FIXME get the realm from the reply!

        # find the Nonce
        idx = reply.find("nonce=",idx)
        if idx < 0:
            return None
        idx_start = idx + 6
        idx_end = reply.find(",",idx)
        if idx_end < 0:
            idx_end = reply.find("\r\n",idx)
            if idx_end < 0:
                return None
        nonce = reply[idx_start + 1:idx_end - 1]

        return (realm,nonce)

    # This function only calculate the digest response
    # not the specific header field.  So another format
    # is required to format the header into the target
    # HTTP digest authentication

    def _calDigest(self, realm, nonce):
        return md5(':'.join((
            md5(':'.join((self._uname,realm,self._pwd))).hexdigest(),
            nonce,
            md5("PLAY:/" + self._mac).hexdigest()
        ))).hexdigest()

    def _formatDigestHeader(self,realm,nonce):
        return self._digestAuthTemplate % (self._uname,
            realm,
            nonce,
            "/%s" % (self._mac),
            self._calDigest(realm,nonce)
        )

    def _requestWithDigest(self,reply):
        ret = self._parseRelamAndNonce(reply)
        if ret is None:
            return reply
        self._params['auth'] = self._formatDigestHeader(ret[0],ret[1])
        data = self._tpl % self._params
        if self._resolution:
            data = self._add_resolution_str(data)
        self._request(data)
        return self._response()

    def _checkAuthorization(self,data):
        return data.find("Unauthorized") < 0

    def _request(self,data):
        while True:
            sz = self._socket.send(data)
            if sz == len(data):
                return
            else:
                data = data[sz:]

    def _response(self):
        ret = ""
        while True:
            try:
                data = self._socket.recv(256)
            except socket.error,e:
                StreamingBackOffTimer.increase("%s:%d" % (self._addr, self._port))
                self._logError("Socket errror %s on URL %s" % (e, self._url))
                if self._socket_reraise:
                    raise
                return "This is not RTSP error but socket error: %s"%(e)

            StreamingBackOffTimer.decrease("%s:%d" % (self._addr, self._port))

            if not data:
                self._logError("Empty RSTP response on URL %s" % self._url)
                return ret
            else:
                ret += data
                if self._checkEOF(ret):
                    return ret

    def __enter__(self):
        self._socket = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        self._socket.connect((self._addr,self._port))
        self._request(self._data)
        reply = self._response()
        if not self._checkAuthorization(reply):
            reply = self._requestWithDigest(reply)
        return (reply, self._url)

    def __exit__(self,type,value,trace):
        self._socket.close()

    def get_socket(self):
        return self._socket


Camera = namedtuple('Camera', ['physicalId', 'id', 'name', 'status'])
Camera.isOnline = lambda self: self.status in ('Online', 'Recording')


class SingleServerRtspTestBase(object):
    """ Provides:
        _fetchCameraList() called from __init__()
        _checkRtspRequest() that checks reply for '200 OK', reporting to stdout and into the log
        _lock is used to avoid different streams' output intersection
        _mkRtspStreamHandler and _mkRtspArchiveHandler - to simplify StreamTcpBasic object creation
     """
    _serverAddr = None
    _serverGUID = None
    _testCase = 0
    _username = None
    _password = None
    _archiveMax = 1
    _archiveMin = 5
    _lock = None
    _allowOffline = False
    _streamingTest = False

    OK_STATUSES = {
        'rtsp': "RTSP/1.0 200 OK",
        'http': "HTTP/1.0 200 OK",
    }

    def __init__(self, archiveMax, archiveMin, serverAddr, serverGUID, uname, pwd, lock):
        self._archiveMax = archiveMax
        self._archiveMin = archiveMin
        self._serverAddr = serverAddr
        self._serverAddrPair = self._serverAddr.split(':',1)
        self._serverGUID = quote_guid(serverGUID)
        self._username = uname
        self._password = pwd
        self._lock = lock
        self._allowOffline = self._streamingTest  # offline cameras are allowed only in this case
        self._fetchCameraList()

    def _addCamera(self, c):
        camera = Camera(c["physicalId"], c["id"], c["name"].encode('utf8'), c['status'])
        self._allCameraList.append(camera)
        if camera.isOnline():
            self._cameraList.append(camera)
            # c['name'] is used for output only and some pseudo-OSes (like Windows) don't have UTF-8 consoles :(
        self._cameraInfoTable[c["id"]] = c

    def _fetchCameraList(self):
        """Gets this server's GUID and filters out the only cameras that should be used on this server
           Fills self._cameraList
        """
        self._cameraList = []
        self._allCameraList = []
        self._cameraInfoTable = dict()
        obj = HttpRequest(self._serverAddr, 'ec2/getCamerasEx', params={'parentId': self._serverGUID.strip('{}')}, printHttpError=Exception)
        log(LOGLEVEL.DEBUG + 9, "\nDEBUG: server %s (use guid %s), getCamerasEx:\n%s" % (self._serverAddr, self._serverGUID, "\n".join(str(c) for c in obj)))
        for c in obj:
            #print "Camera found: %s" % (pprint.pformat(c))
            if c["typeId"] == "{1657647e-f6e4-bc39-d5e8-563c93cb5e1c}":
                continue # Skip desktop
            if "name" in c and c["name"].startswith("ec2_test"):
                continue # Skip fake camera
            self._addCamera(c)

        if not self._allCameraList:
            msg = "Error: no cameras found on server %s"  % (self._serverAddr,)
            with self._lock:
                log(LOGLEVEL.ERROR, msg)
                raise AssertionError(msg)

        if not self._cameraList and not self._allowOffline:
            msg = "Error: no active cameras found on server %s" % (self._serverAddr,)
            with self._lock:
                log(LOGLEVEL.ERROR, msg)
                raise AssertionError(msg)
        #print "DEBUG: server %s, use uid %s, _allCameraList:\n%s" % (self._serverAddr, self._serverGUID, self._allCameraList)

    def _checkReply(self, reply, proto):
        idx = reply.find("\r\n")
        return False if idx < 0 else (self.OK_STATUSES[proto] == reply[:idx])

    def _checkRtspRequest(self, c, reply, proto='rtsp'):
        ret = None
        pName = proto.upper()
        with self._lock:
            log(LOGLEVEL.DEBUG + 9, "%s request on URL: %s issued!" % (pName, reply[1]))
            if not self._checkReply(reply[0], proto):
                log(LOGLEVEL.DEBUG + 9, "%s request on URL %s failed" % (pName, reply[1]))
                log(LOGLEVEL.DEBUG + 9, reply[0])
                log(LOGLEVEL.DEBUG + 9, "Camera name: %s" % (c[2].encode('utf8'),))
                log(LOGLEVEL.DEBUG + 9, "Camera Physical Id: %s" % (c[0]))
                log(LOGLEVEL.DEBUG + 9, "Camera Id: %s" % (c[1]))
                #log(LOGLEVEL.DEBUG + 19, "Detail %s protocol reply:\n%s" % (pName, reply[0]))
                ret = False
            else:
                log(LOGLEVEL.DEBUG + 9, "-------------------------------------")
                log(LOGLEVEL.DEBUG + 9, "%s request on URL %s passed!" % (pName, reply[1]))
                if not self._streamingTest:
                    log(LOGLEVEL.INFO, "%s Test Passed!" % proto.capitalize())
                ret = True
            log(LOGLEVEL.INFO, "-----------------------------------------------------")
            return ret

    def _mkStreamingHandler(self, proto, camera, urlGenerator, socket_reraise=False):
        return StreamTcpBasic(proto, self._serverAddrPair, camera[0:2], self._serverGUID,
                self._username, self._password,
                urlGenerator, self._lock, socket_reraise)

    def _mkLiveStreamHandler(self, proto, camera, socket_reraise=False):
        return self._mkStreamingHandler(proto, camera,
                RtspStreamURLGenerator(self._serverAddrPair, camera.physicalId),
                socket_reraise)

    def _mkArchStreamHandler(self, proto, camera, socket_reraise=False):
        genClass = RtspArchiveURLGenerator if proto == 'rtsp' else HttpArchiveURLGenerator
        return self._mkStreamingHandler(proto, camera,
                genClass(self._archiveMax,self._archiveMin,self._serverAddrPair, camera.physicalId),
                socket_reraise)

    def run(self):
        raise NotImplementedError("ERROR: the abstract method SingleServerRtspTestBase.run isn't overriden in %s" % self.__class__)

# ===================================
# RTSP test
# ===================================
# --- finite test ---

class FiniteSingleServerRtspTest(SingleServerRtspTestBase):
    _testCase = 0
    def __init__(self, archiveMax, archiveMin, serverAddr, serverGUID, testCase, uname, pwd, lock):
        self._testCase = testCase
        SingleServerRtspTestBase.__init__(self, archiveMax, archiveMin, serverAddr, serverGUID,
                                          uname, pwd, lock)

    def _testMain(self):
        if self._cameraList:
            # Streaming version RTSP test
            c = random.choice(self._cameraList)
            with self._mkLiveStreamHandler('rtsp', c) as reply:
                self._checkRtspRequest(c, reply)

        c = random.choice(self._allCameraList)
        with self._mkArchStreamHandler('rtsp', c) as reply:
            self._checkRtspRequest(c, reply)

    def run(self):
        for _ in xrange(self._testCase):
            self._testMain()


class FiniteRtspTest(object):
    """ For each server perform FiniteSingleServerRtspTest
    """

    def __init__(self, config, testSize, userName, passWord, archiveMax, archiveMin):
        self._config = config
        self._testCase = testSize
        self._username = userName
        self._password = passWord
        self._archiveMax = archiveMax # max difference
        self._archiveMin = archiveMin # min difference
        self._lock = threading.Lock()

    def test(self):
        thPool = []
        log(LOGLEVEL.INFO, "-----------------------------------")
        log(LOGLEVEL.INFO, "Finite RTSP test starts")
        log(LOGLEVEL.INFO, "The failed detail result will be logged in rtsp.log file")

        uuidList = self._config.rtget("ServerUUIDList")
        for i, serverAddr in enumerate(self._config.rtget("ServerList")):
            serverAddrGUID = uuidList[i][0]

            tar = FiniteSingleServerRtspTest(self._archiveMax, self._archiveMin, serverAddr, serverAddrGUID,
                                             self._testCase, self._username, self._password, self._lock)

            th = threading.Thread(target = tar.run)
            th.start()
            thPool.append((th, log))

        # Join the thread
        for t in thPool:
            t[0].join()
            t[1].close()

        log(LOGLEVEL.INFO, "Finite RTSP test ends")
        log(LOGLEVEL.INFO, "-----------------------------------")


# --- infinite test ---

class InfiniteSingleServerRtspTest(SingleServerRtspTestBase):
    _flag = None

    def __init__(self, archiveMax, archiveMin, serverAddr, serverGUID, uname, pwd, lock, flag):
        SingleServerRtspTestBase.__init__(self,
                                          archiveMax, archiveMin, serverAddr, serverGUID,
                                          uname, pwd, lock)
        self._flag = flag

    def run(self):
        l = self._serverAddr.split(":")
        results = {False: 0, True: 0} # count results, key is a self._checkRtspRequest call result
        while self._flag.isOn():
            for c in self._allCameraList:
                # Streaming version RTSP test
                if c.isOnline():
                    with self._mkLiveStreamHandler('rtsp', c) as reply:
                        results[self._checkRtspRequest(c, reply)] += 1

                with self._mkArchStreamHandler('rtsp', c) as reply:
                    results[self._checkRtspRequest(c, reply)] += 1

        log(LOGLEVEL.INFO, "-----------------------------------")
        log(LOGLEVEL.INFO, "On server %s\nRTSP Passed: %d\nRTSP Failed: %d" % (self._serverAddr, results[True], results[False]))
        log(LOGLEVEL.INFO, "-----------------------------------\n")


class InfiniteRtspTest(object):
    def __init__(self, config, userName, passWord, archiveMax, archiveMin):
        self._config = config
        self._username = userName
        self._password = passWord
        self._archiveMax = archiveMax # max difference
        self._archiveMin = archiveMin # min difference
        self._lock = threading.Lock()
        self._flag = True

    def isOn(self):
        return self._flag

    def turnOff(self):
        self._flag = False

    def _onInterrupt(self,a,b):
        self.turnOff()

    def test(self):
        thPool = []

        log(LOGLEVEL.INFO, "-------------------------------------------")
        log(LOGLEVEL.INFO, "Infinite RTSP test starts")
        log(LOGLEVEL.INFO, "You can press CTRL+C to interrupt the tests")
        log(LOGLEVEL.INFO, "The failed detail result will be logged in rtsp.log file")

        # Setup the interruption handler
        signal.signal(signal.SIGINT,self._onInterrupt)

        uuidList = self._config.rtget("ServerUUIDList")
        for i, serverAddr in enumerate(self._config.rtget("ServerList")):
            serverAddrGUID = uuidList[i][0]

            tar = InfiniteSingleServerRtspTest(self._archiveMax, self._archiveMin,
                                               serverAddr, serverAddrGUID,
                                               self._username, self._password,
                                               self._lock, self)

            th = threading.Thread(target=tar.run)
            th.start()
            thPool.append((th, log))


        # This is a UGLY work around that to allow python get the interruption
        # while execution.  If I block into the join, python seems never get
        # interruption there.
        while self.isOn():
            try:
                time.sleep(0.5)
            except:
                self.turnOff()
                break

        # Afterwards join them
        for t in thPool:
            t[0].join()
            t[1].close()

        log(LOGLEVEL.INFO, "Infinite RTSP test ends")
        log(LOGLEVEL.INFO, "-------------------------------------------")


class RtspTestSuit(object):

    def __init__(self, config):
        self._config = config
        #if not self._cluster.unittestRollback:
        #    self._cluster.init_rollback()

    def run(self):
        username = self._config.get("General","username")
        password = self._config.get("General","password")
        testSize = self._config.getint("Rtsp","testSize")
        diffMax = self._config.getint("Rtsp","archiveDiffMax")
        diffMin = self._config.getint("Rtsp","archiveDiffMin")

        if testSize < 0 :
            InfiniteRtspTest(self._config, username, password, diffMax, diffMin).test()
        else:
            FiniteRtspTest(self._config, testSize, username, password, diffMax, diffMin).test()


# ================================================================================================
# RTSP performance Operations
# ================================================================================================

class SingleServerRtspPerf(SingleServerRtspTestBase):
    _archiveStreamRate = _DEFAULT_ARCHIVE_STREAM_RATE
    # _timeoutMin and _timeoutMax specify the borders of possible single request data dransmition duration
    _timeoutMax = 0 # whole number of milliseconds
    _timeoutMin = 0 # whole number of milliseconds
    _threadNum = 0
    _exitFlag = None
    _threadPool = None

    _archiveNumOK = 0
    _archiveNumFail = 0
    _archiveNumTimeout = 0
    _archiveNumClose = 0
    _archiveNumSocketError = 0
    _archiveNumEmpty = 0  # also counted as OK
    _streamNumOK = 0
    _streamNumFail = 0
    _streamNumTimeout = 0
    _streamNumClose = 0
    _streamNumSocketError = 0
    _maxWebmRequest = 2
    _webmRequestNum = 0
    _need_dump = False
    _tcpTimeout = 3
    _httpTimeout = 5
    _threadStartGap = _DEFAULT_THREAD_START_GAP  # a gap between new thread of requests starts
    _socketCloseGrace = _DEFAULT_SOCKET_CLOSE_GRACE  # additional time for remote socket to close before open it again
    _camerasStartGrace = _DEFAULT_CAMERAS_START_GRACE  # pause duration to allow all cameras to start recording
    _liveDataPart = _DEFAULT_LIVE_DATA_PART  # a percentage of live data requests (another part are archive data requests)

    _startTime = 0
    _logNameTpl = "%s:%s"

    @classmethod
    def set_global(cls, name, value):
        setattr(cls, '_'+name, value)

    def __init__(self, archiveMax, archiveMin, serverAddr, guid, username, password, threadNum, flag, lock):
        SingleServerRtspTestBase.__init__(self,
                                          archiveMax, archiveMin,
                                          serverAddr, guid,
                                          username, password,
                                          lock)
        self._threadNum = threadNum
        self._exitFlag = flag
        # Order cameras to start recording and preserve a time gap for starting
        self._camerasReadyTime = time.time() + (
            self._camerasStartGrace if self._startRecording() else 0)

    def perfLog(self, msg):
        if self._logNameTpl:
            srvPrefix = self._logNameTpl % tuple(self._serverAddrPair)
            log(LOGLEVEL.DEBUG, '%s: %s' % (srvPrefix, msg))

    def _startRecording(self):
        if self._liveDataPart == 0:
            return False
        log(LOGLEVEL.INFO, "Start recording for all available cameras.") #TODO probably it's good to place it into SingleServerRtspTestBase and call it there.
        cameras = []
        for ph_id, id, name, status in self._cameraList:
            if status != 'Recording':
                attr_data = CAMERA_ATTR_EMPTY.copy()
                attr_data[CAMERA_ID_FIELD] = id
                attr_data['scheduleEnabled'] = True
                attr_data['scheduleTasks'] = FULL_SCHEDULE_TASKS
                cameras.append(attr_data)

        if cameras:
            url = "http://%s/ec2/saveCameraUserAttributesList" % (self._serverAddr,)
            try:
                response = urllib2.urlopen(urllib2.Request(
                        url, data=json.dumps(cameras), headers={'Content-Type': 'application/json'}),
                    timeout=self._httpTimeout
                )
            except Exception as err:
                log(LOGLEVEL.DEBUG + 9, "DEBUG: _startRecording: saveCameraUserAttributesList: %s" % (pprint.pformat(cameras)))
                raise AssertionError("Error from %s: %s" % (url, str(err)))
            if response.getcode() != 200:
                raise AssertionError("Error %s: %s" % (url, response.getcode()))
        return len(cameras) > 0

    def _timeoutRecv(self, socket, rate_limit, timeout):
        """ Read some bytes from the socket, no more then RATE, no longer then TIMEOUT
            If RATE == -1, just read one portion of data
        """
        socket.setblocking(0)
        buf = []
        total_size = 0
        finish = time.time() + timeout

        try:
            while time.time() < finish:
                # recording the time for fetching an event
                ready = select.select([socket], [], [], timeout)
                if ready[0]:
                    data = socket.recv(1024*16) if rate_limit < 0 else socket.recv(rate_limit)
                    if rate_limit == -1:
                        return data
                    buf.append(data)
                    total_size += len(data)
                    if total_size > rate_limit:
                        extra = total_size - rate_limit * (time.time() - self._startTime)
                        if extra > 0:
                            time.sleep(extra/rate_limit) # compensate the rate of packet size here
                        return ''.join(buf)
                else:
                    # timeout reached
                    return None
            # time limit reached
            return ''.join(buf)
        finally:
            socket.setblocking(1)

    def _dumpArchiveHelper(self, c, tcp_rtsp, timeout, dump_file):
        self._startTime = time.time()
        finish = self._startTime + timeout
        dataCount = 0
        last_data = ''
        while time.time() < finish and self._exitFlag.isOn():
            try:
                data = self._timeoutRecv(tcp_rtsp.get_socket(), self._archiveStreamRate, self._tcpTimeout)
                if dump_file is not None:
                    dump_file.write(data)
                    dump_file.flush()
            except Exception:
                logException()
            else:
                if data is None or data == '':
                    with self._lock:
                        self.perfLog("--------------------------------------------")
                        if data is None:
                            log(LOGLEVEL.INFO, "URL %s: no archive data response for %s seconds" % (
                                tcp_rtsp._url, self._tcpTimeout))
                        else:
                            log(LOGLEVEL.INFO, "URL %s: connection has been closed by server after %.2f seconds" % (
                                tcp_rtsp._url, time.time() - self._startTime))
                            if dataCount:
                                if dataCount == 7 and last_data == "0\r\n\r\n\r\n":
                                    log(LOGLEVEL.INFO, "No chunks found!")
                                    self._archiveNumEmpty += 1
                                    return
                        if dataCount:
                            N = 256
                            log(LOGLEVEL.INFO,  "# %s bytes received. last_data %s bytes: %r" % (
                                dataCount, N, last_data[-N:]))
                        self.perfLog("--------------------------------------------")
                        if data is None:
                            self.perfLog("! URL %s no data response for %s seconds" % (
                                tcp_rtsp._url, self._tcpTimeout))
                        else:
                            self.perfLog("! URL %s: connection has been CLOSED by server" % (
                                tcp_rtsp._url,))
                        self.perfLog("Camera name:%s" % c.name)
                        self.perfLog("Camera Physical Id:%s" % c.physicalId)
                        self.perfLog("Camera Id:%s" % c.id)
                        self.perfLog("--------------------------------------------")
                    if data is None:
                        self._archiveNumTimeout += 1
                    elif not dataCount:
                        self._archiveNumClose += 1
                    self._archiveNumOK -= 1
                    return
                else:
                    dataCount += len(data)
                    last_data = data
        with self._lock:
            log(LOGLEVEL.INFO,  "--------------------------------------------")
            log(LOGLEVEL.INFO, "The %.3f seconds streaming from %s finished" % (timeout,tcp_rtsp._url))
            #print ": %s bytes received" % dataCount

    def _dumpStreamHelper(self, c, tcp_rtsp, timeout, dump_file):
        self._startTime = time.time()
        finish = self._startTime + timeout
        while time.time() < finish and self._exitFlag.isOn():
            try:
                data = self._timeoutRecv(tcp_rtsp.get_socket(), -1, self._tcpTimeout)
                if dump_file is not None:
                    dump_file.write(data)
                    dump_file.flush()
            except Exception:
                logException()
            else:
                if data is None or data == '':
                    with self._lock:
                        log(LOGLEVEL.INFO, "--------------------------------------------")
                        if data is None:
                            log(LOGLEVEL.INFO, "Url %s: no live data response for %s seconds" % (
                                tcp_rtsp._url, self._tcpTimeout))
                        else:
                            log(LOGLEVEL.INFO, "Url %s: connection has been closed by server after %.2f seconds" % (
                                tcp_rtsp._url, time.time() - self._startTime))
                        self.perfLog("--------------------------------------------")
                        if data is None:
                            self.perfLog("! Url %s: no data response for %s seconds" % (
                                tcp_rtsp._url, self._tcpTimeout))
                        else:
                            self.perfLog("! Url %s: connection has been CLOSED by server" % (
                                tcp_rtsp._url,))
                        self.perfLog("Camera name:%s" % c.name)
                        self.perfLog("Camera Physical Id:%s" % c.physicalId)
                        self.perfLog("Camera Id:%s" % c.id)
                        self.perfLog("--------------------------------------------")
                    if data is None:
                        self._streamNumTimeout += 1
                    else:
                        self._streamNumClose += 1
                    self._streamNumOK -= 1
                    return
        with self._lock:
            log(LOGLEVEL.INFO, "--------------------------------------------")
            log(LOGLEVEL.INFO, "The %.3f seconds streaming from %s finished" % (timeout,tcp_rtsp._url))

    def _dump(self,c, tcp_rtsp, timeout, helper):
        if self._need_dump:
            with open(_buildUrlPath(tcp_rtsp._url),"w+") as f:
                helper(c, tcp_rtsp, timeout, f)
        else:
            helper(c, tcp_rtsp, timeout, None)

    def _makeDataReceivePeriod(self):
        return random.randint(self._timeoutMin, self._timeoutMax) / 1000.0

    # Represent a streaming TASK on the camera
    def _main_streaming(self, c):
        try:
            obj = self._mkLiveStreamHandler('rtsp', c, socket_reraise=True)
            obj.add_prefered_resolution(random.choice(['low', 'high']))
            with obj as reply:
                # 1.  Check the reply here
                if self._checkRtspRequest(c, reply, 'rtsp'):
                    self._dump(c, tcp_rtsp=obj, timeout=self._makeDataReceivePeriod(), helper=self._dumpStreamHelper)
                    self._streamNumOK += 1
                else:
                    self._streamNumFail += 1
        except socket.error:
            log(LOGLEVEL.INFO, "--------------------------------------------")
            log(LOGLEVEL.INFO, "The RTSP url %s test fails with the socket error %s" % (obj._url, sys.exc_info() ))
            self._streamNumSocketError += 1
        except Exception:
            log(LOGLEVEL.INFO, "--------------------------------------------")
            log(LOGLEVEL.INFO, "A live streaming test fails with exception:\n%s" % (traceback.format_exc(), ))
            self._streamNumFail += 1

    def _main_archive(self, c):
        try:
            proto = random.choice(('rtsp', 'http'))
            # webm requests limitation
            if proto == 'http':
               self._webmRequestNum+=1
            if self._webmRequestNum > self._maxWebmRequest:
                proto = 'rtsp'
            obj = self._mkArchStreamHandler(proto, c, socket_reraise=True)
            if proto == 'rtsp':
                obj.add_prefered_resolution(random.choice(['low', 'high']))
            with obj as reply:
                # 1.  Check the reply here
               if self._checkRtspRequest(c, reply, proto):
                   self._dump(c, tcp_rtsp=obj, timeout=self._makeDataReceivePeriod(), helper=self._dumpArchiveHelper)
                   self._archiveNumOK += 1
               else:
                   self._archiveNumFail += 1
        except socket.error:
            log(LOGLEVEL.INFO,  "--------------------------------------------")
            log(LOGLEVEL.INFO, "The RTSP url %s test fails with the socket error %s" % (obj._url, sys.exc_info() ))
            self._archiveNumSocketError += 1
        except Exception:
            log(LOGLEVEL.INFO, "--------------------------------------------")
            log(LOGLEVEL.INFO, "An archive streaming test fails with exception:\n%s" % (traceback.format_exc(), ))
            self._archiveNumFail += 1

    def _threadMain(self, num):
        while self._exitFlag.isOn():
            # choose a random camera in the server list
            c = random.choice(self._allCameraList if self._allowOffline else self._cameraList)
            if random.randint(1,100) <= self._liveDataPart:
                self._main_streaming(c)
            else:
                self._main_archive(c)
            if self._socketCloseGrace:
                time.sleep(self._socketCloseGrace)

    def join(self):
        #print "*** Stack:"
        #traceback.print_stack()
        for th in self._threadPool:
            th.join()
        log(LOGLEVEL.INFO, "=======================================")
        log(LOGLEVEL.INFO, "Server: %s" % (self._serverAddr))
        log(LOGLEVEL.INFO, "Number of threads: %s" % self._threadNum)
        log(LOGLEVEL.INFO, "tcpTimeout value: %s" % self._tcpTimeout)
        if self._liveDataPart < 100:
            log(LOGLEVEL.INFO, "Archive Success Number: %d" % self._archiveNumOK)
            if self._archiveNumEmpty:
                log(LOGLEVEL.INFO, "...empty chunks: %d" % self._archiveNumEmpty)
            log(LOGLEVEL.INFO, "Archive Failed Number: %d" % self._archiveNumFail)
            log(LOGLEVEL.INFO, "Archive Timed Out Number: %d" % self._archiveNumTimeout)
            log(LOGLEVEL.INFO, "Archive Server Closed Number: %d" % self._archiveNumClose)
            log(LOGLEVEL.INFO, "Archive Socket Error Number: %d" % self._archiveNumSocketError)
        if self._liveDataPart > 0:
            log(LOGLEVEL.INFO, "Stream Success Number:%d" % self._streamNumOK)
            log(LOGLEVEL.INFO, "Stream Failed Number:%d" % self._streamNumFail)
            log(LOGLEVEL.INFO, "Stream Timed Out Number: %d" % self._streamNumTimeout)
            log(LOGLEVEL.INFO, "Stream Server Closed Number: %d" % self._streamNumClose)
            log(LOGLEVEL.INFO, "Stream Socket Error Number: %d" % self._streamNumSocketError)
        log(LOGLEVEL.INFO, "=======================================")
        # only archive results are interesting since it's for streaming tests only
        return 0 == self._archiveNumFail + self._archiveNumTimeout + self._archiveNumClose + self._archiveNumSocketError

    def isAlive(self):
        return any(imap(threading.Thread.isAlive, self._threadPool))

    def run(self, need_dump=False):
        if not self._cameraList and not self._allowOffline:
            if self._allCameraList:
                log(LOGLEVEL.INFO, "All cameras on server %s are offline!" % (self._serverAddr,))
            else:
                log(LOGLEVEL.INFO,"The camera list on server: %s is empty!" % (self._serverAddr,))
            log(LOGLEVEL.INFO, "Do nothing and abort!")
            return False
        dt = self._camerasReadyTime - time.time()
        if dt > 0:
            log(LOGLEVEL.DEBUG + 9, "DEBUG: cameras could be unready, sleep %.2f seconds" % dt)
            time.sleep(dt)

        self._need_dump = need_dump
        self._threadPool = []
        for _ in xrange(self._threadNum):
            th = threading.Thread(target=self._threadMain, args=(_,))
            th.start()
            self._threadPool.append(th)
            if self._threadStartGap:
                time.sleep(self._threadStartGap)
        return True

    def getAddr(self):
        return self._serverAddr

# -----------------------------------------------

class RtspPerf(object):
    _cs = 'Rtsp'  # main config section
    _perfServer = []
    _lock = threading.Lock()
    _exit = False
    _streamTest = False
    _singleServerClass = SingleServerRtspPerf

    def __init__(self, config):
        self._config = config
        self._need_dump = config.rtget("need_dump")
        self._serverList = config.rtget("ServerList")
        self._serverUUIDList = config.rtget("ServerUUIDList")
        if type(self._serverUUIDList[0]) == list:
            self._serverUUIDList = [s[0] for s in self._serverUUIDList]
        self._username = self._config.get("General", "username")
        self._password = self._config.get("General", "password")

        self.threadNumbers = config.get(self._cs,"threadNumbers").split(",")
        if len(self.threadNumbers) != len(self._serverList):
            if len(self.threadNumbers) == 1:
                self.threadNumbers = self.threadNumbers * len(self._serverList)
            else:
                self.run = self._cantRun # substitute the `run` method
                return

        #StreamTcpBasic.set_close_timeout_global(config.getint_safe("Rtsp", "rtspCloseTimeout", 5))

        self._singleServerClass.set_global('timeoutMin', config.getint(self._cs,"timeoutMin") * 1000)
        self._singleServerClass.set_global('timeoutMax', config.getint(self._cs,"timeoutMax") * 1000)
        self._singleServerClass.set_global('tcpTimeout', config.getint_safe(self._cs, "tcpTimeout", 3))
        self._singleServerClass.set_global('httpTimeout', config.getint_safe("Rtsp", "httpTimeout", 5))
        self._singleServerClass.set_global('camerasStartGrace',
                                        config.getint_safe(self._cs, "camerasStartGrace", _DEFAULT_CAMERAS_START_GRACE))
        self._singleServerClass.set_global('threadStartGap',
                                        config.getfloat_safe(self._cs, "threadStartGap", _DEFAULT_THREAD_START_GAP))
        self._singleServerClass.set_global('socketCloseGrace',
                                        config.getfloat_safe("Rtsp", "socketCloseGrace", _DEFAULT_SOCKET_CLOSE_GRACE))
        self._singleServerClass.set_global('liveDataPart',
                                        0 if self._streamTest else # FIXME parameter!
                                        config.getint_safe("Rtsp", "liveDataPart", _DEFAULT_LIVE_DATA_PART))

        rate = config.get_safe("Rtsp", "archiveStreamRate", None)
        self._singleServerClass.set_global(
            'archiveStreamRate', _DEFAULT_ARCHIVE_STREAM_RATE if rate is None else parse_size(rate) )

        self._singleServerClass.set_global('streamingTest', self._streamTest)

    def isOn(self):
        return not self._exit

    def turnOff(self):
        self._exit = True

    def _onInterrupt(self,a,b):
        self.turnOff()

    def _cantRun(self):
        log(LOGLEVEL.INFO, "The threadNumbers list in %s section has size > 1 and doesn't match the size of the serverList" % (self._cs,))
        log(LOGLEVEL.INFO, "threadNumbers = %s" % self.threadNumbers)
        log(LOGLEVEL.INFO, "ServerList = %s" % self._serverList)
        if self._streamTest:
            log(LOGLEVEL.INFO, "Streaming test FAILED")
        else:
            log(LOGLEVEL.INFO, "RTSP Pressure test FAILED")

    def initTest(self):
        archiveMin = self._config.getint(self._cs, "archiveDiffMin")
        archiveMax = self._config.getint(self._cs, "archiveDiffMax")

        # Let's add those RtspSinglePerf
        self._perfServer = []
        for i, serverAddr in enumerate(self._serverList):
            self._perfServer.append(self._singleServerClass(
                archiveMax, archiveMin,
                serverAddr, self._serverUUIDList[i],
                self._username, self._password,
                int(self.threadNumbers[i]), self, self._lock,
            ))
        return True

    def _mainSleep(self):
        while self.isOn():
            try:
                time.sleep(1)
                if not any(e.isAlive() for e in self._perfServer):
                    if self.isOn():  # only print if threads are dead by their own will
                        log(LOGLEVEL.INFO, "(All threads are finished. Stopping.)")
                    break
            except Exception:
                break
            except KeyboardInterrupt:
                break
        self.turnOff()  # in case of interruption

    def run(self):
        self.initTest()
        log(LOGLEVEL.INFO, "---------------------------------------------")
        if self._streamTest:
            log(LOGLEVEL.INFO, "Start the streaming test")
        else:
            initLog(15)
            log(LOGLEVEL.INFO, "Start the RTSP pressure test now. Press CTRL+C to interrupt the test!")

        # Add the signal handler
        signal.signal(signal.SIGINT,self._onInterrupt)

        for e in self._perfServer:
            if not e.run(self._need_dump):
                return

        self._mainSleep()

        # all threads should stop since self.isOn() returns False
        fail = False
        for e in self._perfServer:
            if not e.join() and self._streamTest:
                log(LOGLEVEL.ERROR, "FAIL: some requests are unsuccessed on %s" % (e.getAddr(),))
                fail = True

        if self._streamTest:
            log(LOGLEVEL.INFO, "Streaming test %s" % ("FAILED" if fail else "PASSED",))
        else:
            log(LOGLEVEL.INFO, "RTSP performance test done,see log for detail")
        log(LOGLEVEL.INFO, "---------------------------------------------")
        return not fail


class RtspStreamTest(RtspPerf):
    _cs = 'Streaming'
    _streamTest = True
    #def __init__(self, config):
    #    #self._multi = multiproto
    #    RtspPerf.__init__(self, config)

    def _finish(self):
        log(LOGLEVEL.INFO, "[%s] ...Finishing test.." % (int(time.time()),))
        self.turnOff()

    def _mainSleep(self):
        tm = threading.Timer(
            self._config.getint_safe(self._cs, "testDuration", _DEFAULT_STREAMING_DURATION),
            self._finish)
        tm.start()
        super(RtspStreamTest, self)._mainSleep()
        if tm and tm.isAlive():
            tm.cancel()

#-----------------------------------------------------------
class SingleServerHlsTest(SingleServerRtspPerf):
    _logNameTpl = "%s:%s"
    BAD_STATUS_RETRY = 3

    # From httplib.py:
    #   Presumably, the server closed the connection before
    #   sending a valid response.
    # It looks like an httplib issue, we may ignore it and try again.
    def _safe_request(self, request):
        for i in xrange(self.BAD_STATUS_RETRY):
            try:
                return urllib2.urlopen(request, timeout=self._httpTimeout)
            except httplib.BadStatusLine, x:
                if i == self.BAD_STATUS_RETRY - 1:
                    raise

    def _hlsRequest(self, url):
        err = ""
        try:
            response = self._safe_request(
                urllib2.Request(url, headers={'x-server-guid': self._serverGUID}))
        except Exception as e:
            err = "ERROR: url %s request raised exception (%s) %s" % (url, type(e).__name__, e)
        else:
            if response.getcode() != 200:
                err = "ERROR: URL %s returned code %s" % (url, response.getcode())
        if err:
            raise AssertionError(err)
        return response.read()

    def _getStartM3u(self, c, period):
        pos = RandomArchTime(self._archiveMin, self._archiveMax)
        url = "http://%s/hls/%s.m3u?pos=%s" % (self._serverAddr, c.physicalId, pos)
        log(LOGLEVEL.DEBUG + 9, "Requesting %s for %s sec." % (url, period))
        return self._hlsRequest(url)

    def _checkLineUrl(self, line):
        return line.startswith('/hls/')

    def _parseM3u(self, data, chooseOne=False):
        "Parses initial m3u list and choses one of URIs (hi or lo)"
        if data is None:
            raise AssertionError("Empty m3u data from %s" % self._serverAddr)
        uri = []
        dataLines = data.split("\n")
        if dataLines[0].strip() != "#EXTM3U":
            raise AssertionError("Wrong m3u data (no #EXTM3U header) from %s" % self._serverAddr)
        for line in dataLines:
            if self._checkLineUrl(line):
                uri.append(line.rstrip())
        if not uri:
            raise AssertionError("No links found in n3u data from %s. Data:\n%s" % (self._serverAddr, dataLines))
        #print "DEBUG: uris found:\n%s" % "\n".join(uri)
        return random.choice(uri) if chooseOne else uri

    def _loadCunks(self, chunks):
        for uri in chunks:
            url = "http://%s%s" % (self._serverAddr, uri)
            data = self._hlsRequest(url)
            #print "Chunk %s: %s bytes received" % (url, len(data))

    def _main_archive(self, c):
        period = self._makeDataReceivePeriod()
        finish = time.time() + period
        m3u = self._getStartM3u(c, period)
        uri = self._parseM3u(m3u, chooseOne=True)
        #print "Choosen %s" % uri
        url = "http://%s%s" % (self._serverAddr, uri)
        while time.time() < finish:
            chunks = self._parseM3u(self._hlsRequest(url))
            self._loadCunks(chunks)
        self._archiveNumOK += 1

    def _threadMain(self, num):
        self._liveDataPart = 0
        try:
            super(SingleServerHlsTest, self)._threadMain(num)
        except Exception as e:
            logException()
            self._archiveNumFail += 1

class HlsStreamingTest(RtspStreamTest):
    _singleServerClass = SingleServerHlsTest

#    def initTest(self):
#        super(HlsStreamingTest, self).initTest()

if __name__ == '__main__':
    print "%s not supposed to be executed alone." % (__file__,)
