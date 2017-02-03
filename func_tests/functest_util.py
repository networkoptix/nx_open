__author__ = 'Danil Lavrentyuk'
"This module contains some utility functions and classes for the functional tests script."
import sys
import json
from urllib import urlencode
import urllib2
from ConfigParser import RawConfigParser
import traceback
import Queue
import threading
import difflib
from pycommons.Logger import log, LOGLEVEL

__all__ = ['JsonDiff', 'FtConfigParser', 'compareJson', 'showHelp', 'getHelpDesc',
           'TestDigestAuthHandler', 'ManagerAddPassword',
           'SafeJsonLoads', 'TestJsonLoads', 'checkResultsEqual', 'textdiff',
           'ClusterWorker', 'ClusterLongWorker', 'parse_size', 'real_caps',
           'CAMERA_ATTR_EMPTY', 'FULL_SCHEDULE_TASKS',
           'sendRequest', 'TestRequestError', 'LegacyTestFailure', 'ServerCompareFailure']

class LegacyTestFailure(AssertionError):
    pass

class ServerCompareFailure(LegacyTestFailure):
    pass


class TestRequestError(AssertionError):

    def __init__(self, url, errMessage, data=None):
        super(TestRequestError, self).__init__(url, errMessage, data)
        self.url = url
        self.errMessage = errMessage
        self.message = "Failed to request %s: %s" % (url, errMessage)
        if data is not None:
            self.message += "\nPosted data was: %s" % (data,)

# Empty camera's attributes structure
CAMERA_ID_FIELD = 'cameraId'
PREFERRED_SERVER_ID_FIELD = "preferredServerId"
CAMERA_ATTR_EMPTY = {
    CAMERA_ID_FIELD: '',
    'scheduleEnabled': '',
    'backupType': '',  # CameraBackup_HighQuality, CameraBackup_LowQuality or CameraBackupBoth
    'cameraName': '',
    'userDefinedGroupName': '',
    'licenseUsed': '',
    'motionType': '',
    'motionMask': '',
    'scheduleTasks': '',
    'audioEnabled': '',
    'secondaryStreamQuality': '',
    'controlEnabled': '',
    'dewarpingParams': '',
    'minArchiveDays': '',
    'maxArchiveDays': '',
    PREFERRED_SERVER_ID_FIELD: '',
    'failoverPriority': ''
}

# Schedule to activate camera recording
FULL_SCHEDULE_TASKS = [
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 1,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    },
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 2,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    },
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 3,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    },
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 4,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    },
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 5,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    },
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 6,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    },
    {
        "afterThreshold": 5,
        "beforeThreshold": 5,
        "dayOfWeek": 7,
        "endTime": 86400,
        "fps": 15,
        "recordAudio": False,
        "recordingType": "RT_Always",
        "startTime": 0,
        "streamQuality": "highest"
    }
]

def fixApi(api):
    "Fixes some API names"
    global CAMERA_ID_FIELD, PREFERRED_SERVER_ID_FIELD
    if api.cameraId != CAMERA_ID_FIELD:
        CAMERA_ATTR_EMPTY[api.cameraId] = CAMERA_ATTR_EMPTY.pop(CAMERA_ID_FIELD)
        CAMERA_ID_FIELD = api.cameraId
    if api.preferredServerId  != PREFERRED_SERVER_ID_FIELD:
        CAMERA_ATTR_EMPTY[api.preferredServerId] = CAMERA_ATTR_EMPTY.pop(PREFERRED_SERVER_ID_FIELD)
        PREFERRED_SERVER_ID_FIELD = api.preferredServerId


# ---------------------------------------------------------------------
# A deep comparison of json object
# This comparision is slow , but it can output valid result when 2 json
# objects has different order in their array
# ---------------------------------------------------------------------
class JsonDiff:
    _hasDiff = False
    _errorInfo=""
    _anchorStr=None
    _currentRecursionSize=0
    _anchorStack = []
    _switched = False

    def switchSides(self):
        self._switched = not self._switched

    def isSwitched(self):
        return self._switched

    def keyNotExisted(self,lhs,rhs,key):
        """ format the error information based on key lost """
        self._errorInfo = ("CurrentPosition:{anchor}\n"
                     "The key:{k} is not existed in both two objects\n").format(anchor=self._anchorStr,k=key)
        self._hasDiff = True
        return self

    def keysDiffer(self, lhs, rhs):
        " format the error information on key sets difference"
        self._errorInfo = ("CurrentPosition:{anchor}\n"
            "Different keys found. {lk} in one object and {rk} in the other\n"
        ).format(anchor=self._anchorStr,
                 lk=str(list(lhs.viewkeys()-rhs.viewkeys())),
                 rk=str(list(rhs.viewkeys()-lhs.viewkeys()))
                 )
        self._hasDiff = True
        return self

    def listIndexNotFound(self, lhs, idx):
        """ format the error information based on the array index lost"""
        self._errorInfo= ("CurrentPosition: {anchor}\n"
                    "The element in a {side} list at index:{i} cannot be found in other objects\n"
                    "Element: {e}"
            ).format(anchor=self._anchorStr, i=idx, e=lhs[idx], side='right' if self._switched else 'left')
        self._hasDiff = True
        return self

    def leafValueNotSame(self,lhs,rhs):
        lhs_str = None
        rhs_str = None
        try :
            lhs_str = str(lhs)
        except Exception:
            lhs_str = repr(lhs)
        try:
            rhs_str = str(rhs)
        except:
            rhs_str = repr(rhs)

        self._errorInfo= ("CurrentPosition:{anchor}\n"
            "The left hand side value:{lval} and right hand side value:{rval} is not same"
            ).format(anchor=self._anchorStr, lval=lhs_str, rval=rhs_str)
        self._hasDiff = True
        return self

    def typeNotSame(self,lhs,rhs):
        self._errorInfo=("CurrentPosition:{anchor}\n"
            "The left hand value type:{lt} is not same with right hand value type:{rt}\n"
            ).format(anchor=self._anchorStr, lt=type(lhs), rt=type(rhs))
        self._hasDiff = True
        return self

    def enter(self,position):
        if self._anchorStr is None:
            self._anchorStack.append(0)
        else:
            self._anchorStack.append(len(self._anchorStr))

        if self._anchorStr is None:
            self._anchorStr=position
        else:
            self._anchorStr+=".%s"%(position)

        self._currentRecursionSize += 1

    def leave(self):
        assert len(self._anchorStack) != 0 , "No place to leave"
        val = self._anchorStack[len(self._anchorStack)-1]
        self._anchorStack.pop(len(self._anchorStack)-1)
        self._anchorStr = self._anchorStr[:val]
        self._currentRecursionSize -= 1

    def hasDiff(self):
        return self._hasDiff

    def errorInfo(self):
        return self._errorInfo if self._hasDiff else "<no diff>"

    def resetDiff(self):
        self._hasDiff = False
        self._errorInfo=""


class FtConfigParser(RawConfigParser):

    def __init__(self, *args, **kw_args):
        RawConfigParser.__init__(self, *args, **kw_args)
        self.runtime = dict()

    def get_safe(self, section, option, default=None):
        if not self.has_option(section, option):
            return default
        return self.get(section, option)

    def getint_safe(self, section, option, default=None):
        if not self.has_option(section, option):
            return default
        return self.getint(section, option)

    def getfloat_safe(self, section, option, default=None):
        if not self.has_option(section, option):
            return default
        return self.getfloat(section, option)

    def rtset(self, name, value):
        self.runtime[name] = value

    def rtget(self, name):
        return self.runtime[name]

    def rthas(self, name):
        return name in self.runtime


def _compareJsonObject(lhs,rhs,result):
    assert isinstance(lhs,dict),"The lhs object _MUST_ be an object"
    assert isinstance(rhs,dict),"The rhs object _MUST_ be an object"
    # compare the loop and stuff
    if lhs.viewkeys() ^ rhs.viewkeys():
        return result.keysDiffer(lhs, rhs)
    for lhs_key in lhs.iterkeys():
        if lhs_key not in rhs:
            return result.keyNotExisted(lhs,rhs,lhs_key);
        else:
            lhs_obj = lhs[lhs_key]
            rhs_obj = rhs[lhs_key]
            result.enter(lhs_key)
            if _compareJson(lhs_obj,rhs_obj,result).hasDiff():
                result.leave()
                return result
            result.leave()

    return result


def _compareJsonList(lhs, rhs, result):
    assert isinstance(lhs,list),"The lhs object _MUST_ be a list"
    assert isinstance(rhs,list),"The rhs object _MUST_ be a list"

    # The list comparison should ignore element's order. A
    # naive O(n*n) comparison is used here. For each element P in lhs,
    # it tries to find an element in rhs. If not found -- fail.
    # Then it checks if there is no any element in rhs that
    # isn't already checked by lhs.

    if len(lhs) < len(rhs):
        switched = True
        result.switchSides()
        (lhs, rhs) = (rhs, lhs)
    else:
        switched = False

    tabooSet = set()

    try:
        for lhs_idx,lhs_ele in enumerate(lhs):
            notFound = True
            for idx,val in enumerate(rhs):
                if idx in tabooSet:
                    continue
                # now checking if this value has same value with the lhs_ele
                if not _compareJson(lhs_ele, val, result).hasDiff():
                    tabooSet.add(idx)
                    notFound = False
                    break
                else:
                    result.resetDiff()

            if notFound:
                return result.listIndexNotFound(lhs, lhs_idx)
    finally:
        if switched:
            result.switchSides()

    return result


def _compareJsonLeaf(lhs,rhs,result):
    if isinstance(rhs,type(lhs)):
        if rhs != lhs:
            return result.leafValueNotSame(lhs,rhs)
        else:
            return result
    else:
        return result.typeNotSame(lhs,rhs)


def _compareJson(lhs,rhs,result):
    lhs_type = type(lhs)
    rhs_type = type(rhs)
    if lhs_type != rhs_type:
        return result.typeNotSame(lhs,rhs)
    else:
        if lhs_type is dict:
            # Enter the json object here
            return _compareJsonObject(lhs,rhs,result)
        elif rhs_type is list:
            return _compareJsonList(lhs,rhs,result)
        else:
            return _compareJsonLeaf(lhs,rhs,result)


def compareJson(lhs,rhs):  # :type: JsonDiff
    result = JsonDiff()
    # An outer most JSON element must be an array or dict
    if isinstance(lhs,list):
        if not isinstance(rhs,list):
            return result.typeNotSame(lhs,rhs)
    else:
        if not isinstance(rhs,dict):
            return result.typeNotSame(lhs,rhs)

    result.enter("<root>")
    if _compareJson(lhs,rhs,result).hasDiff():
        return result
    result.leave()
    return result


def checkResultsEqual(responseList, methodName):
    """responseList - is a list of pairs (response, address).
    The function compares that all responces are ok and their json contents are equal.
    Returns a tupple of a boolean success indicator and a string fail reason.
    """
    log(LOGLEVEL.INFO, "------------------------------------------")
    log(LOGLEVEL.INFO, "Check sync status on method %s" % (methodName))
    result = None
    resultAddr = None
    resultJsonObject = None

    for entry in responseList:
        response, address = entry[0:2]

        if response.getcode() != 200:
            raise LegacyTestFailure("Server: %s method: %s HTTP request failed with code: %d" %
                                    (address, methodName, response.getcode()))

        content = response.read()
        if result is None:
            result = content
            resultAddr = address
            resultJsonObject = TestJsonLoads(result, resultAddr, methodName)
        else:
            if content != result:
                # Since the server could issue json object has different order which makes us
                # have to do deep comparison of json object internally. This deep comparison
                # is very slow and only performs on objects that has failed the naive comparison
                contentJsonObject = TestJsonLoads(content, address, methodName)
                compareResult = compareJson(contentJsonObject, resultJsonObject)
                if compareResult.hasDiff():
                    log(LOGLEVEL.ERROR, "Method %s returns different results on server %s and %s" % (methodName, address, resultAddr))
                    log(LOGLEVEL.ERROR, compareResult.errorInfo())
                    raise ServerCompareFailure("Servers %s and %s aren't synced on %s" % (address, resultAddr, methodName))
        response.close()
    log(LOGLEVEL.INFO, "Method %s is synced in cluster" % (methodName))
    log(LOGLEVEL.INFO, "------------------------------------------")


# ---------------------------------------------------------------------
# Print help (general page or function specific, depending in sys.argv
# ---------------------------------------------------------------------
_helpMenu = {
    "perf":("Run performance test",(
        "Usage: python main.py --perf --type=... \n\n"
        "This command line will start built-in performance test\n"
        "The --type option is used to specify what kind of resource you want to profile.\n"
        "Currently you could specify Camera and User, eg : --type=Camera,User will test on Camera and User both;\n"
        "--type=User will only do performance test on User resources")),
    "clear":("Clear resources",(
        "Usage: python main.py --clear \nUsage: python main.py --clear --fake\n\n"
        "This command is used to clear the resource in server list.\n"
        "The resource includes Camera,MediaServer and Users.\n"
        "The --fake option is a flag to tell the command _ONLY_ clear\n"
        "resource that has name prefixed with \"ec2_test\".\n"
        "This name pattern typically means that the data is generated by the automatic test\n")),
    "sync":("Test cluster is sycnchronized or not",(
        "Usage: python main.py --sync \n\n"
        "This command is used to test whether the cluster has synchronized states or not.")),
    "recover":("Recover from previous fail rollback",(
        "Usage: python main.py --recover \n\n"
        "This command is used to try to recover from previous failed rollback.\n"
        "Each rollback will based on a file .rollback. However rollback may failed.\n"
        "The failed rollback transaction will be recorded in side of .rollback file as well.\n"
        "Every time you restart this program, you could specify this command to try\n "
        "to recover the failed rollback transaction.\n"
        "If you are running automatic test, the recover will be detected automatically and \n"
        "prompt for you to choose whether recover or not")),
    "merge-test":("Run merge test",(
        "Usage: python main.py --merge-test \n\n"
        "This command is used to run merge test speicifically.\n"
        "This command will run admin user password merge test and resource merge test.\n")),
    "merge-admin":("Run merge admin user password test",(
        "Usage: python main.py --merge-admin \n\n"
        "This command is used to run run admin user password merge test directly.\n"
        "This command will be removed later on")),
    "rtsp-test":("Run rtsp test",(
        "Usage: python main.py --rtsp-test \n\n"
        "This command is used to run RTSP Test test.It means it will issue RTSP play command,\n"
        "and wait for the reply to check the status code.\n"
        "User needs to set up section in functest.cfg file: [Rtsp]\ntestSize=40\n"
        "The testSize is configuration parameter that tell rtsp the number that it needs to perform \n"
        "RTSP test on _EACH_ server. Therefore, the above example means 40 random RTSP test on each server.\n")),
    "add":("Resource creation",(
        "Usage: python main.py --add=... --count=... \n\n"
        "This command is used to add different generated resources to servers.\n"
        "3 types of resource is available: MediaServer,Camera,User. \n"
        "The --add parameter needs to be specified the resource type. Eg: --add=Camera \n"
        "means add camera into the server, --add=User means add user to the server.\n"
        "The --count option is used to tell the size that you wish to generate that resources.\n"
        "Eg: main.py --add=Camera --count=100           Add 100 cameras to each server in server list.\n")),
    "remove":("Resource remove",(
        "Usage: python main.py --remove=Camera --id=... \n"
        "Usage: python main.py --remove=Camera --fake \n\n"
        "This command is used to remove resource on each servers.\n"
        "The --remove needs to be specified required resource type.\n"
        "3 types of resource is available: MediaServer,Camera,User. \n"
        "The --id option is optinoal, if it appears, you need to specify a valid id like this:--id=SomeID.\n"
        "It is used to delete specific resource. \n"
        "Optionally, you could specify --fake flag , if this flag is on, then the remove will only \n"
        "remove resource that has name prefixed with \"ec2_test\" which typically means fake resource")),
    "rtsp-perf":("Rtsp performance test",(
        "Usage: python main.py --rtsp-perf \n\n"
        "Usage: python main.py --rtsp-perf --dump \n\n"
        "This command is used to run rtsp performance test.\n"
        "The test will try to check RTSP status and then connect to the server \n"
        "and maintain the connection to receive RTP packet for several seconds. The request \n"
        "includes archive and real time streaming.\n"
        "Additionally,an optional option --dump may be specified.If this flag is on, the data will be \n"
        "dumped into a file, the file stores raw RTP data and also the file is named with following:\n"
        "{Part1}_{Part2}, Part1 is the URL ,the character \"/\" \":\" and \"?\" will be escaped to %,$,#\n"
        "Part2 is a random session number which has 12 digits\n"
        "The configuration parameter is listed below:\n\n"
        "threadNumbers    A comma separate list to specify how many list each server is required \n"
        "The component number must be the same as component in serverList. Eg: threadNumbers=10,2,3 \n"
        "This means that the first server in serverList will have 10 threads,second server 2,third 3.\n\n"
        "archiveDiffMax       The time difference upper bound for archive request, in minutes \n"
        "archiveDiffMin       The time difference lower bound for archive request, in minutes \n"
        "timeoutMax           The timeout upper bound for each RTP receiving, in seconds. \n"
        "timeoutMin           The timeout lower bound for each RTP receiving, in seconds. \n"
        "Notes: All the above parameters needs to be specified in configuration file: functest.cfg under \n"
        "section Rtsp.\nEg:\n[Rtsp]\nthreadNumbers=10,2\narchiveDiffMax=..\nardchiveDiffMin=....\n"
        )),
    "sys-name":("System name test",(
        "Usage: python main.py --sys-name \n\n"
        "This command will perform system name test for each server.\n"
        "The system name test is , change each server in cluster to another system name,\n"
        "and check each server that whether all the other server is offline and only this server is online.\n"
        )),
    "log": ("Log redirection option", (
        "With --log FILE all tests' output is redirected into a file.\n"
        "Only short test result message is sent to stdout.\n"
        "Using --log without a  FILE name makes it print all output in the end of test\n"
        "only if the test fails.\n"
        )),
    }

def showHelp(arg):
    if not arg:
        print """Help for auto test tool

*****************************************
************* Function Menu *************
*****************************************
Entry            Introduction
"""
        maxitemlen = max(len(s) for s in _helpMenu.iterkeys())
        for k,v in _helpMenu.iteritems():
            print "%s  %s" % ( k.ljust(maxitemlen) + ':', v[0])

        print """

To see detail help information, please run command:
python main.py --help Entry

Eg: python main.py --help auto-test
This will list detail information about auto-test
"""
    else:
        if arg in _helpMenu:
            print "==================================="
            print arg
            print "===================================\n"
            print _helpMenu[arg][1]
            print
        else:
            print "Option: %s is not found !" % (arg,)


def getHelpDesc(topic, full=False):
    if topic in _helpMenu:
        return _helpMenu[topic][int(full)]
    else:
        return "<description not found>"

###########################################

class TestDigestAuthHandler(urllib2.HTTPDigestAuthHandler):
    "Used to avoid AbstractDigestAuthHandler.retried counter usage in http_error_auth_reqed"

    def http_error_auth_reqed(self, auth_header, host, req, headers):
        authreq = headers.get(auth_header, None)
        if authreq:
            scheme = authreq.split()[0]
            if scheme.lower() == 'digest':
                return self.retry_http_digest_auth(req, authreq)


# A helper function to unify pasword managers' configuration
def ManagerAddPassword(passman, host, user, pwd):
    passman.add_password(None, "http://%s/ec2" % (host), user, pwd)
    passman.add_password(None, "http://%s/api" % (host), user, pwd)
    passman.add_password(None, "http://%s/hls" % (host), user, pwd)
    passman.add_password(None, "http://%s/proxy" % (host), user, pwd)


def SafeJsonLoads(text, serverAddr, methodName):
    try:
        return json.loads(text)
    except ValueError, e:
        log(LOGLEVEL.ERROR, "Error parsing server %s, method %s response: %s" % (serverAddr, methodName, e))
        return None

def TestJsonLoads(text, serverAddr, methodName):
    try:
        data = json.loads(text)
        if data is None:
            raise ValueError()
        return data
    except ValueError:
        raise LegacyTestFailure("Wrong response from %s on %s" % (serverAddr, methodName))

def HttpRequest(serverAddr, methodName, params=None, headers=None, timeout=None, printHttpError=False, logURL=False):
    url = "http://%s/%s" % (serverAddr, methodName)
    err = ""
    if params:
        url += '?'+ urlencode(params)
    if logURL:
        log(LOGLEVEL.DEBUG + 9, "Requesting: " + url)
    req = urllib2.Request(url)
    if headers:
        for k, v in headers.iteritems():
            req.add_header(k, v)
    try:
        response = urllib2.urlopen(req) if timeout is None else urllib2.urlopen(req, timeout=timeout)
    except Exception as e:
        err = "ends with exception %s" % e
    else:
        if response.getcode() != 200:
            err = "returns %s HTTP code" % (response.getcode(),)
    if err:
        if printHttpError:
            if params:
                err = "Error: url %s %s" % (url, err)
            else:
                err = "Error: server %s, method %s %s" % (serverAddr, methodName, err)
            if isinstance(printHttpError, Exception):
                raise printHttpError(err)
            log(LOGLEVEL.ERROR, err)
        return None
    data = response.read()
    log(LOGLEVEL.DEBUG + 9, "DEBUG0: %s returned data: %s" % (url, repr(data)))
    if len(data):
        return SafeJsonLoads(data, serverAddr, methodName)
    return True


#def safe_request_json(req):
#    try:
#        return json.loads(urllib2.urlopen(req).read())
#    except Exception as e:
#        if isinstance(req, urllib2.Request):
#            req = req.get_full_uri()
#        print "FAIL: error requesting '%s': %s" % (req, e)
#        return None


# Thread queue for multi-task.  This is useful since if the user
# want too many data to be sent to the server, then there maybe
# thousands of threads to be created, which is not something we
# want it.  This is not a real-time queue, but a push-sync-join
class ClusterWorker(object):
    _queue = None
    _threadList = None
    _threadNum = 1
    _prestarted = False
    _working = False

    def __init__(self, num, queue_size=0, doStart=False, startEvent = None):
        self._threadNum = num
        if queue_size == 0:
            queue_size = num
        elif queue_size < num:
            self._threadNum = queue_size
        self._queue = Queue.Queue(queue_size)
        self._threadList = []
        self._oks = []
        self._fails = []
        self._startEvent = startEvent
        if doStart:
            self._prestarted = True
            self.startThreads()

    def _do_work(self):
        return self._prestarted or not self._queue.empty()

    def _worker(self, num):
        while self._do_work():
            func, args = self._queue.get(True)
            if self._startEvent: self._startEvent.wait()
            try:
                func(*args)
            except LegacyTestFailure as err:
                msg = "%s failed: %s"  % (func.__name__, err.message)
                log(LOGLEVEL.ERROR, "ERROR: ClusterWorker call " + msg)
                self._oks.append(False)
                self._fails.append(msg)
            except Exception as err:
                etype, value, tb = sys.exc_info()
                log(LOGLEVEL.ERROR, "ERROR: ClusterWorker call to %s failed with %s: %s\nTraceback:\n%s" % (
                    func.__name__, type(err).__name__, getattr(err, 'message', str(err)),
                    ''.join(traceback.format_tb(tb))))
                self._oks.append(False)
            else:
                self._oks.append(True)
            finally:
                self._queue.task_done()

    def startThreads(self):
        for _ in xrange(self._threadNum):
            t = threading.Thread(target=self._worker, args=(_,))
            t.daemon = False
            t.start()
            self._threadList.append(t)

    def joinThreads(self):
        for t in self._threadList:
            t.join()

    def joinQueue(self):
        self._queue.join()

    def workDone(self):
        return self._queue.empty()

    def join(self):
        # We delay the real operation until we call join
        if self._prestarted:
            self._prestarted = False
        if not self._threadList:
            self.startThreads()
        alive = [th for th in self._threadList if th.isAlive()]
        if len(alive) < len(self._threadList):
            self._threadList[:] = alive
        if not (alive) and self._queue.qsize() > 0:
            log(LOGLEVEL.WARNING, "WARNING: no alive threads but queue isn't empty! Starting threads again!")
            self._threadList.clear()
            self.startThreads()
        # Second we call queue join to join the queue
        self.joinQueue()
        # Now we safely join the thread since the queue
        # will utimatly be empty after execution
        self.joinThreads()

    def enqueue(self, task, args):
        self._queue.put((task, args), True)

    def allOk(self):
        return all(self._oks)

    def clearOks(self):
        self._oks = []

    def getFails(self):
        return self._fails[:]

    def getFailsMsg(self):
        return 'Some worker tasks failed:' + (''.join('\n\t' + msg for msg in self._fails))


class ClusterLongWorker(ClusterWorker):

    def __init__(self, num, queue_size=0):
        #print "ClusterLongWorker starting"
        super(ClusterLongWorker, self).__init__(num, queue_size)
        self._working = False

    def _do_work(self):
        return self._working

    def startThreads(self):
        self._working = True
        super(ClusterLongWorker, self).startThreads()

    def stopWork(self):
        self.enqueue(self._terminate, ())
        self.joinThreads()
        while not self._queue.empty():
            try:
                self._queue.get_nowait()
                self._queue.task_done()
            except Queue.Empty:
                break

    def _terminate(self):
        self._working = False
        self.enqueue(self._terminate, ()) # for other threads


def quote_guid(guid):
    return guid if guid[0] == '{' else "{" + guid + "}"


def unquote_guid(guid):
    return guid[1:-1] if guid[0] == '{' and guid[-1] == '}' else guid


def get_server_guid(host):
    info = HttpRequest(host, "api/moduleInformation", printHttpError=True)
    if info and (u'id' in info['reply']):
        return unquote_guid(info['reply'][u'id'])
    return None


class Version(object):
    value = []

    def __init__(self, verStr=''):
        self.value = verStr.split('.')

    def __str__(self):
        return '.'.join(self.value)

    def __cmp__(self, other):
        return cmp(self.value, other.value)


def parse_size(size_str):
    "Parse string like 100M, 20k to a number of bytes, i.e. 100M = 100*1024*1024 bytes"
    if size_str[-1].upper() == 'K':
        mult = 1024
        size_str = size_str[:-1].rstrip()
    elif size_str[-1].upper() == 'M':
        mult = 1024*1024
        size_str = size_str[:-1].rstrip()
    else:
        mult = 1
    return int(size_str) * mult


def real_caps(str):
    "String's method capitalize makes lower all chars except the first. If it isn't desired - use real_caps."
    return (str[0].upper() + str[1:]) if len(str) else str


def textdiff(data0, data1, src0, src1):
    ud = difflib.unified_diff(data0.splitlines(True), data1.splitlines(True), src0, src1, n=5)
    return ''.join(line if line.endswith('\n') else line+'\n' for line in ud)


def sendRequest(lock, url, data, notify=False):
    req = urllib2.Request(url, data=data, headers={'Content-Type': 'application/json'})
    try:
        with lock:
            response = urllib2.urlopen(req)
            if notify:
                if data:
                    log(LOGLEVEL.DEBUG + 9, "Requesting '%s': '%s'" % (url, data))
                else:
                    log(LOGLEVEL.DEBUG + 9, "Requesting '%s'" % url)
    except Exception as err:
        raise TestRequestError(url, str(err), data)
    if response.getcode() != 200:
        raise TestRequestError(url, "HTTP error code %s" % (response.getcode(),), data)
    response.close()


# Create keys for api/mergeSystems call

import hashlib, base64, urllib
def generateMehodKey(method, user, digest, nonce):
    m = hashlib.md5()
    nedoHa2 = m.update("%s:" % method)
    nedoHa2 = m.hexdigest()
    m = hashlib.md5()
    m.update(digest)
    m.update(':')
    m.update(nonce)
    m.update(':')
    m.update(nedoHa2)
    authDigest = m.hexdigest()
    return urllib.quote(base64.urlsafe_b64encode("%s:%s:%s" % (user.lower(), nonce, authDigest)))

def generateKey(method, user, password, nonce, realm):
    m = hashlib.md5()
    m.update("%s:%s:%s" % (user.lower(), realm, password) )
    digest = m.hexdigest()
    return generateMehodKey(method, user, digest, nonce)
