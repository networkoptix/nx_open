# $Id$
# Artem V. Nikitin
# Fill mediaserver archieve storage

import os, time, shutil
from Config import config, DEFAULT_SECTION
from Utils import currentTime
from Rec import Rec

HI_QUALITY_DIR = 'hi_quality'
LOW_QUALITY_DIR = 'low_quality'

# TODO. Find python module for avi duration detection
def detectAVIDuration(aviFile):
    import subprocess, re
    result = subprocess.Popen(["ffprobe", aviFile],
                       stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    ds = [x for x in result.stdout.readlines() if "Duration" in x]
    if not ds:
        raise Exception("Can't detect '%s' duration" % aviFile)
    # Duration: 00:01:05.72
    m = re.search(r'Duration:\s+(\d+):(\d{2}):(\d{2})\.(\d+)', ds[0])
    if not m:
        raise Exception("Can't detect '%s' duration" % aviFile)
    duration = int(m.group(1))*60*60*1000
    duration += int(m.group(2))*60*1000
    duration += int(m.group(3))*1000
    duration += int(m.group(4))
    return duration

# Storage file sample:
#   data/hi_quality/92-61-00-00-00-01/2017/01/19/08/1484813007543_5531.mkv
def ftime2path(ftime):
    sec = ftime / 1000
    lt = time.localtime(sec)
    year = str(lt.tm_year)
    month = "%02d" % lt.tm_mon
    day = "%02d" % lt.tm_mday
    hour = "%02d" % lt.tm_hour
    return os.path.join(year, month, day, hour)

def copyFile(srcFile, dstFile):
    dstDir = os.path.dirname(dstFile)
    if not os.path.exists(dstDir):
        os.makedirs(dstDir)
    shutil.copy(srcFile, dstFile)

class FillSettings(object):

    def __init__(self, sampleFile = None,
                 startTime = time.time(),
                 step = 0, count = 1):
        self.sampleFile = sampleFile or \
           os.path.join(config.get(DEFAULT_SECTION, "vagrantFolder"), "sample.mkv")
        self.startTime = startTime
        self.step = step
        self.count = count

def fillStorage(dataDir, cameraMac, settings):
    sampleDuration = detectAVIDuration(settings.sampleFile)
    hi_path = os.path.join(dataDir, 'hi_quality', cameraMac)
    low_path = os.path.join(dataDir, 'low_quality', cameraMac)
    startTime = settings.startTime
    startTime = int(startTime * 1000) # Convert to milliseconds
    startTime -= settings.count * (settings.step + 1) * sampleDuration
    fileTimes = []
    for i in xrange(settings.count):
        fileTime = startTime + i * (settings.step + 1) * sampleDuration
        fileTimes.append(
            Rec(startTimeMs = str(fileTime),
                durationMs = str(sampleDuration)) )
        fileSubDir = ftime2path(fileTime)
        fileName = "%s_%s.mkv" % (fileTime, sampleDuration)
        hiFilePath = os.path.join(hi_path, fileSubDir, fileName)
        lowFilePath = os.path.join(low_path, fileSubDir, fileName)
        copyFile(settings.sampleFile, hiFilePath)
        copyFile(settings.sampleFile, lowFilePath)
    return fileTimes

REBUILD_TIMEOUT = 10.0

class StorageMixin:

    def rebuildArchive(self, server):
        # Start archive rebuild
        self.sendAndCheckRequest(
            server.address,
            'api/rebuildArchive',
            action = 'start',
            mainPool = 1)

        # Check rebuild is comlete
        start = time.time()
        while True:
            response = self.sendAndCheckRequest(
                server.address,
                'api/rebuildArchive')
            if response.data['reply']["state"] == 'RebuildState_None':
                break
            remain = REBUILD_TIMEOUT - (time.time() - start)
            if remain <= 0:
                self.fail("Archive rebuild timeout")            
            time.sleep(1.0)

                           

                           
        
