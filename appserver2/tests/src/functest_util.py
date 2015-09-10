__author__ = 'Danil Lavrentyuk'
"This module contains some utility functions and classes for the functional tests script."
import sys
import json

__all__ = ['JsonDiff', 'compareJson', 'showHelp', 'ManagerAddPassword', 'SafeJsonLoads']

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

    def keyNotExisted(self,lhs,rhs,key):
        """ format the error information based on key lost """
        self._errorInfo = ("CurrentPosition:{anchor}\n"
                     "The key:{k} is not existed in both two objects\n").format(anchor=self._anchorStr,k=key)
        self._hasDiff = True
        return self

    def arrayIndexNotFound(self,lhs,idx):
        """ format the error information based on the array index lost"""
        self._errorInfo= ("CurrentPosition:{anchor}\n"
                    "The element in array at index:{i} cannot be found in other objects\n").\
            format(anchor=self._anchorStr,i=idx)
        self._hasDiff = True
        return self

    def leafValueNotSame(self,lhs,rhs):
        lhs_str = None
        rhs_str = None
        try :
            lhs_str = str(lhs)
            rhs_str = str(rhs)
        except:
            lhs_str = lhs.__str__()
            rhs_str = rhs.__str__()

        self._errorInfo= ("CurrentPosition:{anchor}\n"
                    "The left hand side value:{lval} and right hand side value:{rval} is not same").format(anchor=self._anchorStr,
                                                                                                           lval=lhs_str,
                                                                                                           rval=rhs_str)
        self._hasDiff = True
        return self

    def typeNotSame(self,lhs,rhs):
        ltype = type(lhs)
        rtype = type(rhs)

        self._errorInfo=(
            "CurrentPosition:{anchor}\n"
            "The left hand value type:{lt} is not same with right hand value type:{rt}\n").format(anchor=self._anchorStr,
                                                                                                  lt=ltype,
                                                                                                  rt=rtype)
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
        if self._hasDiff:
            return self._errorInfo
        else:
            return "<no diff>"


    def resetDiff(self):
        self._hasDiff = False
        self._errorInfo=""


def _compareJsonObject(lhs,rhs,result):
    assert isinstance(lhs,dict),"The lhs object _MUST_ be an object"
    assert isinstance(rhs,dict),"The rhs object _MUST_ be an object"
    # compare the loop and stuff
    for lhs_key in lhs:
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


def _compareJsonList(lhs,rhs,result):
    assert isinstance(lhs,list),"The lhs object _MUST_ be a list"
    assert isinstance(rhs,list),"The rhs object _MUST_ be a list"

    # The comparison of list should be ignore the element's order. A
    # naive O(n*n) comparison is used here. For each element P in lhs,
    # it will try to match one element in rhs , if not failed, otherwise
    # passed.

    tabooSet = set()

    for lhs_idx,lhs_ele in enumerate(lhs):
        notFound = True
        for idx,val in enumerate(rhs):
            if idx in tabooSet:
                continue
            # now checking if this value has same value with the lhs_ele
            if not _compareJson(lhs_ele,val,result).hasDiff():
                tabooSet.add(idx)
                notFound = False
                continue
            else:
                result.resetDiff()

        if notFound:
            return result.arrayIndexNotFound(lhs,lhs_idx)

    return result


def _compareJsonLeaf(lhs,rhs,result):
    lhs_type = type(lhs)
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


def compareJson(lhs,rhs):
    result = JsonDiff()
    # An outer most JSON element must be an array or dict
    if isinstance(lhs,list):
        if isinstance(rhs,list):
            result.enter("<root>")
            if _compareJson(lhs,rhs,result).hasDiff():
                return result
            result.leave()

        else:
            return result.typeNotSame(lhs,rhs)
    else:
        if isinstance(rhs,dict):
            result.enter("<root>")
            if _compareJson(lhs,rhs,result).hasDiff():
                return result
            result.leave()
        else:
            return result.typeNotSame(lhs,rhs)
    #FIXME check if lhs neother list nor dit!

    return result


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
        "User needs to set up section in ec2_tests.cfg file: [Rtsp]\ntestSize=40\n"
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
    "auto-test":("Automatic test",(
        "Usage: python main.py \n\n"
        "This command is used to run built-in automatic test.\n"
        "The automatic test includes 11 types of test and they will be runed automatically."
        "The configuration parameter is as follow: \n"
        "threadNumber                  The thread number that will be used to fire operations\n"
        "mergeTestTimeout              The timeout for merge test\n"
        "clusterTestSleepTime          The timeout for other auto test\n"
        "All the above configuration parameters needs to be defined in the General section.\n"
        "The test will try to rollback afterwards and try to recover at first.\n"
        "Also the sync operation will be performed before any test\n")),
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
        "Notes: All the above parameters needs to be specified in configuration file:ec2_tests.cfg under \n"
        "section Rtsp.\nEg:\n[Rtsp]\nthreadNumbers=10,2\narchiveDiffMax=..\nardchiveDiffMin=....\n"
        )),
    "sys-name":("System name test",(
        "Usage: python main.py --sys-name \n\n"
        "This command will perform system name test for each server.\n"
        "The system name test is , change each server in cluster to another system name,\n"
        "and check each server that whether all the other server is offline and only this server is online.\n"
        ))
    }

def showHelp():
    if len(sys.argv) == 2:
        helpStrHeader=("Help for auto test tool\n\n"
                 "*****************************************\n"
                 "**************Function Menu**************\n"
                 "*****************************************\n"
                 "Entry            Introduction            \n")

        print helpStrHeader

        maxitemlen = max(len(s) for s in _helpMenu.iterkeys())+1
        for k,v in _helpMenu.iteritems():
            print "%s   %s" % ( (k+':').ljust(maxitemlen), v[0])

        helpStrFooter = ("\n\nTo see detail help information, please run command:\n"
               "python main.py --help Entry\n\n"
               "Eg: python main.py --help auto-test\n"
               "This will list detail information about auto-test\n")

        print helpStrFooter
    else:
        option = sys.argv[2]
        if option in _helpMenu:
            print "==================================="
            print option
            print "===================================\n\n"
            print _helpMenu[option][1]
        else:
            print "Option: %s is not found !"%(option)


# A helper function to unify pasword managers' configuration
def ManagerAddPassword(passman, host, user, pwd):
    passman.add_password(None, "http://%s/ec2" % (host), user, pwd)
    passman.add_password(None, "http://%s/api" % (host), user, pwd)


def SafeJsonLoads(text, serverAddr, methodName):
    try:
        return json.loads(text)
    except ValueError, e:
        print "Error parsing server %s, method %s response: %s" % (serverAddr, methodName, e)
        return None
