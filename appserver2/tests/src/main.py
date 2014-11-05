import unittest
import urllib2
import urllib
import ConfigParser
import time
import thread

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
    """description of class"""
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

    def setUp(self):
        self.loadConfig()

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
        testCaseList=cfg_parser.get("ClusterTest","testCaseFile").split(",")
        for testCaseFile in testCaseList:
            config=ConfigParser.RawConfigParser()
            config.read(testCaseFile)
            all_sec=config.sections()
            for section in all_sec:
                self.testCaseList.append(config.items(section))

    # This function will check whether each element in returnList has same value.
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

    def checkMethodStatusConsistent(self,method):
        responseList=[]
        for server in self.serverList:
            responseList.append(
                    (server[self.SERVER_NAME],
                        urllib2.urlopen( "http://%s:%d/ec2/%s"%(server[self.SERVER_ADDR],server[self.SERVER_PORT],method))
                        ))
        # checking the last response validation 
        self.checkResultEqual(responseList,method)


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

    def formatTestCaseParam(self,testCase):
        method = self.getTestCaseListItem(testCase,"method")
        if method == None:
            return False
        observer = self.getTestCaseListItem(testCase,"observer")
        if observer == None:
            return False

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

        parameters={}
        for entry in testCase:
            if (entry[0] == "method") or (entry[0] == "observer") or (entry[0] == "server") or (entry[0] == "timeout"):
                continue
            else:
                parameters[entry[0]]=entry[1]

        url_appending = urllib.urlencode(parameters)

        return ("http://%s:%d/ec2/%s?%s"%(server[self.SERVER_ADDR],server[self.SERVER_PORT],method,url_appending),
                method,
                observer,
                timeout,
                server[self.SERVER_NAME])

    def sendRequest(self,url,timeout):
        # issue the query request here
        response = urllib2.urlopen(url)
        if response == None:
            return False
        if response.getcode() != 200:
            return False
        return response

    def waitForResponse(self,observer,timeout):
        time.sleep(timeout/1000)
        self.checkMethodStatusConsistent(observer)
        

    # Iterating all the dynamic execution in the configuration list one by one
    # Generally, a modification test is formed as following structure:
    # (1) A modification
    # (2) Wait for some time
    # (3) Checking all the machine are on the same page

    def test_2(self):
        for testCase in self.testCaseList:
            arguments = self.formatTestCaseParam(testCase)
            if arguments == False:
                continue
            self.assertTrue(self.sendRequest( arguments[0], arguments[3]),
                            "Cannot send request:%s to server:%s"%(arguments[1],arguments[4]))
            self.waitForResponse(arguments[2],arugments[3])


    # This test is used to ensure that at very first stage, all the server has exactly same 
    # states. Since the execution order of each test case is based on the string order so ,
    # test_XX will make sure the execution order. This method is not very elegant , but works.

    def test_1(self):
        for method in self.ec2GetRequests:
            self.checkMethodStatusConsistent(method)

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
