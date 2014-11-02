import unittest
import urllib2
import ConfigParser


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
