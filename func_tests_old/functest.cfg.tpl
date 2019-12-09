[General]
# Server list is a comma separated ip:port address list, eg : 123.123.123.123:12345,124.124.124.124:12345
#serverList=127.0.0.1:7001
serverList=%(serverList)s
vagrantFolder=%(vagrantFolder)s
clusterTestSleepTime=5
mergeTestTimeout=10
threadNumber=32
username=%(username)s
password=%(password)s
testCaseSize=10
#
[Nat]
serverList=%(natTestServerList)s
#
[Rtsp]
# comma separated list, number of threads for each server from serverList:
threadNumbers=100
# which servers to use from the general serverList, 0-based list indexes (not implemented)
useServers=0
# How far into the past ask for archive data. Min and max offset (in _minutes_) for random moment generation.
# Offset == 0 means live data stream request.
archiveDiffMax=100
archiveDiffMin=2
timeoutMax=20
timeoutMin=5
# numer of test for --rtsp-test, comment it to run infinite test:
testSize=10
httpTimeout=10
tcpTimeout=3
camerasStartGrace=10
#rtspCloseTimeout=5
# pause after each SingleServerRtspPerf thread start, seconds:
threadStartGap=0.01
# extra time after closing a socket before opening another one (let a server perform it's after-socket-close job), seconds:
socketCloseGrace=0.11
# Don't receive more than this amount of archive data per second:
archiveStreamRate=1M
# Percentage of live data requests in the test (the other part is archive data)
# A whole number from 0 to 100 (0 means only archive, 100 -- only live), default is 50
liveDataPart=30
#
[PerfTest]
# This value means how many operations will be issued AT MOST in one seconds, the bigger the more
frequency=100
#
[Streaming]
# After running for testDuration seconds, the streaming test stops creating new queries
# and wait for all issued queries to finish, then finishes itself.
testDuration=50
threadNumbers=8
threadStartGap=0.1
archiveDiffMin=15
archiveDiffMax=120
# How long do receive each request result: random time period between min and max, seconds
timeoutMin=10
timeoutMax=30
tcpTimeout=6
camerasStartGrace=3
