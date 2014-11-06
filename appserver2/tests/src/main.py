import unittest
import urllib2
import urllib
import ConfigParser
import time
import threading
import json

#class TestSequenceFunctions(unittest.TestCase):

#    def setUp(self):
#        self.seq = range(10)

#    def test_shuffle(self):
#        # make sure the shuffled sequence does not lose any elements
#        random.shuffle(self.seq)
#        self.seq.sort()
#        self.assertEqual(self.seq, range(10))

#        # should raise an exception for an immutable sequence
#        self.assertRaises(TypeError, random.shuffle, (1,2,3))

#    def test_choice(self):
#        element = random.choice(self.seq)
#        self.assertTrue(element in self.seq)

#    def test_sample(self):
#        with self.assertRaises(ValueError):
#            random.sample(self.seq, 20)
#        for element in random.sample(self.seq, 5):
#            self.assertTrue(element in self.seq)


ec2GetRequests = [
    "getResourceTypes",
    "getResourceParams",
    "getMediaServers",
    "getMediaServersEx",
    "getCameras",
    "getCamerasEx",
    "getCameraHistoryItems",
    "getCameraBookmarkTags",
    "getBusinessRules",
    "getUsers",
    "getVideowalls",
    "getLayouts",
    "listDirectory",
    "getStoredFile",
    "getSettings",
    "getCurrentTime",
    "getFullInfo",
    "getLicenses"
]

serverAddress = None
serverPort = None

class EnumerateAllEC2GetCallsTest(unittest.TestCase):

    def test_callAllGetters(self):
        for reqName in ec2GetRequests:
            response = urllib2.urlopen("http://%s:%d/ec2/%s" % (serverAddress,serverPort,reqName))
            self.assertTrue(response.getcode() == 200, "%s failed with statusCode %d" % (reqName,response.getcode()))
            print "%s OK\r\n" % (reqName)



class TestEC2ServerModification(unittest.TestCase):
    serverList=()

    ec2GetRequests = [
    "getResourceTypes",
    "getResourceParams",
    "getMediaServers",
    "getMediaServersEx",
    "getCameras",
    "getCamerasEx",
    "getCameraHistoryItems",
    "getCameraBookmarkTags",
    "getBusinessRules",
    "getUsers",
    "getVideowalls",
    "getLayouts",
    "listDirectory",
    "getStoredFile",
    "getSettings",
    "getFullInfo",
    "getLicenses"
    ]

    SERVER_NAME = 0
    SERVER_ADDR = 1
    SERVER_PORT = 2

    testCaseList=[]
    serverStatesSyncTimeout=0

    def setUp(self):
        self.loadConfig()
        self.ensureServerListStates()
        print "Start executing cluster tests\n"

    def loadConfig(self):
        # reading the config 
        cfg_parser = ConfigParser.RawConfigParser()
        ret = cfg_parser.read("ec2_cluster_test.cfg")
        if not ret:
            raise Exception("No configuration file:ec2_cluster_test.cfg")

        # get the server list from the config file
        serverListStr = cfg_parser.get("ClusterTest","serverList")

        # split it into the list
        serverList = serverListStr.split(",")
        self.serverList=[]

        # now for each list
        for server in serverList:
            self.serverList.append((
                server,
                cfg_parser.get(server,"serverAddress"),
                cfg_parser.getint(server,"port")))
                
        # load other config
        testCaseList=cfg_parser.get("ClusterTest","testCaseList").split(",")

        # the test case list is a dictionary groupped by the group name
        for testCaseFile in testCaseList:
            # Each test case file is a group of test case, the test inside of a 
            # group test file will be executed in parallel instead of one by one
            caseList=[]
            config=ConfigParser.RawConfigParser()
            config.read(testCaseFile)
            all_sec=config.sections()
            for section in all_sec:
                caseList.append( (section,config.items(section)) )
            # insert the case list to the group list
            self.testCaseList.append((testCaseFile,caseList))

        # loading the timeout for server status synchronization 
        self.serverStatesSyncTimeout = cfg_parser.getint("ClusterTest","serverStatusSyncTimeout")

    # This function will check whether each element in returnList has same values.
    # The expected element in returnList is urlib2 response object, and once we
    # find out there'are difference in returnList, we'll just return false otherwise
    # we will return true here

    def checkResultEqual(self,responseList,methodName):
        result=None
        for response in responseList:
            if response[1].getcode() != 200:
                self.assertTrue(false,"The server:%s method:%s http request failed with code:%d"%
                    (response[0],methodName,response[1].getcode))
            else:
                # painfully slow, just bulk string comparison here
                if result == None:
                    result=response[1].read()
                else:
                    output = response[1].read()
                    if result != output:
                        self.assertTrue(false,"The server:%s method:%s return value differs from other"%
                            (response[0],methodName))

    def checkMethodStatusConsistent(self,method,method_list):
        responseList=[]
        url_query=None

        if method_list != None:
            url_query = urllib.urlencode(
                method_list)

        for server in self.serverList:
            if url_query == None:
                responseList.append(
                        (server[self.SERVER_NAME],
                            urllib2.urlopen( 
                                "http://%s:%d/ec2/%s"%(
                                    server[self.SERVER_ADDR],
                                    server[self.SERVER_PORT],
                                    method))))
            else:
                responseList.append(
                        (server[self.SERVER_NAME],
                            urllib2.urlopen( 
                                "http://%s:%d/ec2/%s?%s"%(
                                    server[self.SERVER_ADDR],
                                    server[self.SERVER_PORT],
                                    method,
                                    url_query))))

        # checking the last response validation 
        self.checkResultEqual(responseList,method)

    def ensureServerListStates(self):
        time.sleep(self.serverStatesSyncTimeout)
        for method in self.ec2GetRequests:
            self.checkMethodStatusConsistent(method,None)

        print "All test servers have same states\n"

    def getServerInfo(self,serverName):
        for server in self.serverList:
            if server[self.SERVER_NAME] == serverName:
                return server
        return None

    def getTestCaseListItem(self,testCase,key):
        for entry in testCase:
            if entry[0] == key:
                return entry[1]

        return None


    def parseMethods(self,method):
        methods_list = method.split(';')
        ret=[]
        for entry in methods_list:
            entry = entry.split(',')

            post_data=None
            method_name=None

            if len(entry) == 2 :
                f = open(entry[1],'r')
                post_data = f.read()
                f.close()

            method_name = entry[0]

            ret.append((
                "http://%s:%d/ec2/%s"%(serverAddress,serverPort,method_name),
                method_name,
                post_data))

        return ret

    def parseObserver(self,observer):
        observer_list=observer.split(',')
        query_list = None

        if len(observer_list) == 2:
            query_list=dict()
            l=observer_list[1].split(';')
            for query in l:
                kv=query.split(':')
                query_list[kv[0]]=kv[1]

        return (observer_list[0],query_list)

    def formatTestCaseParam(self,testCase):
        method = self.getTestCaseListItem(testCase,"method")
        if method == None:
            return False

        method_list = self.parseMethods(method)

        observer = self.getTestCaseListItem(testCase,"observer")
        if observer == None:
            return False

        observer = self.parseObserver(observer)

        server = self.getTestCaseListItem(testCase,"server")
        if server == None:
            return False
        else:
            server = self.getServerInfo(server)
            if server == None:
                return False

        timeout = self.getTestCaseListItem(testCase,"timeout")
        if timeout == None:
            return False
        # convert it into the integer 
        timeout = int(timeout)

        return (method_list,
                observer,
                timeout,
                server[self.SERVER_NAME])

    def sendRequest(self,url,timeout,post_data,tc_name,req_name,ser_name):
        # issue the query request here
        response=None

        if post_data != None:
            req = urllib2.Request(url, data=post_data, headers={'Content-type': 'application/json'})
            response = urllib2.urlopen(req)
        else:
            response = urllib2.urlopen(url)

        if response == None:
            print "Test case:%s failed.\nThe request:%s for server:%s failed with connection" \
                %(tc_name,req_name,ser_name)
            return False

        if response.getcode() != 200:
            print "Test case:%s failed.\nThe request:%s for server:%s failed with http error code:%d" \
                %(tc_name,req_name,ser_name,response.getcode())
            return False

        data = response.read()

        if data != '' and data != None:
            ret_obj = json.loads( data )
            if "error" in ret_obj:
                # The request failed
                print "Test case:%s failed.\nThe request:%s for server:%s failed " \
                        "with error:%s"%(tc_name,req_name,ser_name,ret_obj["errorString"])
                return False

        return True


    def waitForResponse(self,observer,timeout):
        time.sleep(timeout)
        self.checkMethodStatusConsistent(observer[0],observer[1])

    def workOnSingleTestCase(self,test_case_name,tc):
        arg = self.formatTestCaseParam(tc)
        if arg == False:
            return
        
        methods_list=arg[0]
        observer=arg[1]
        timeout=arg[2]
        server_name=arg[3]

        # sending out all the request one by one here
        for methods in methods_list:
            # Sending out the request
            self.assertTrue(self.sendRequest(methods[0],timeout,methods[2], \
                test_case_name,methods[1],server_name),"Failed")

        self.waitForResponse(observer,timeout)

    def runSingleTestCase(self,test_case_name,tc):
        # Execute the test case in another thread
        th = threading.Thread(target=self.workOnSingleTestCase,args=(test_case_name,tc,))
        th.start()
        return th

    def runSingleTestGroup(self,group_list):
        # executing the test group locally here
        print "Start executing test group:%s\n"%(group_list[0])

        thread_list=[]
        # the group_list[1][0] is the test case name
        for tc in group_list[1]:
            thread_list.append(self.runSingleTestCase(tc[0],tc[1]))

        # join all the thread test here
        for th in thread_list:
            th.join()

        print "Test group:%s execution finished"%(group_list[0])
    
    # Iterating all the dynamic execution in the configuration list one by one
    # Generally, a modification test is formed as following structure:
    # (1) A modification
    # (2) Wait for some time
    # (3) Checking all the machine are on the same page

    def test_runAllTestCase(self):
        for group_case in self.testCaseList:
            self.runSingleTestGroup(group_case)

if __name__ == '__main__':
    # reading config
    config = ConfigParser.RawConfigParser()
    config.read("ec2_tests.cfg")

    serverAddress = config.get("General", "serverAddress")
    serverPort = config.getint("General", "port")
    serverUser = config.get("General", "user")
    serverPassword = config.get("General", "password")

    passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
    passman.add_password( "NetworkOptix", "http://%s:%d/ec2/" % (serverAddress, serverPort), serverUser, serverPassword )
    urllib2.install_opener(urllib2.build_opener(urllib2.HTTPDigestAuthHandler(passman)))

    unittest.main()
