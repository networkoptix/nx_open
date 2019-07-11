import os
import sys
import time
import argparse
import plistlib
import datetime
        
def getRequestUUID(requestResult):
    requestUUIDData = requestResult.get("notarization-upload", None)
    return requestUUIDData["RequestUUID"] if requestUUIDData else None

def getUpploadRequestErrors(requestResult):
    errors = requestResult.get("product-errors")
    return plistlib.writePlistToString(errors) if errors else None
    
def parseUploadResult(requestOutput):
    requestResult = plistlib.readPlistFromString(requestOutput)
    requestId = getRequestUUID(requestResult)
    return [requestId, ""] if requestId else [None, getUpploadRequestErrors(requestResult)]

def execute(command):
    pipe = os.popen(command)
    output = pipe.read()
    return [not pipe.close(), output]

def uploadForNotarization(options):
    commandTemplate = "xcrun altool --notarize-app --output-format xml -f {0} --primary-bundle-id {1} -itc_provider {2} -u {3} -p {4}"
    command = commandTemplate.format(options.dmgFileName, options.bundleId, options.teamId, options.user, options.password)
    [success, output] = execute(command)
    return parseUploadResult(output)

def checkNotarizationCompletion(options):
    commandTemplate = "xcrun altool --notarization-info {0} -u {1} -p {2} --output-format xml"
    command = commandTemplate.format(options.requestId, options.user, options.password)
    [success, output] = execute(command)
    progressData = plistlib.readPlistFromString(output)
    info = progressData.get("notarization-info", None)
    if (not success) or (not info):
        return [False, False, "Unexpected error: no data returned"]
    
    status = info.get("Status", None)
    if not status:
        return [False, False, "Enexpected error: no status field"]
    
    if status == "in progress":
        return [False, False, "In progress"]
    
    if status == "success":
        return [True, True, "Success"]
    
    return [True, False, "See errors in:\n{}".format(info.get("LogFileURL"))]

def waitForNotarizationCompletion(options, checkPeriodSeconds):
    startTime = time.time()
    while True:
        [completed, success, error] = checkNotarizationCompletion(options)
        if not completed:        
            time.sleep(checkPeriodSeconds)
            if (time.time() - startTime) > (options.timeout * 60):
                return False, "Operation timeout"
            continue
            
        return success, error

def stapleApp(dmgFileName):
    command = "xcrun stapler staple {}".format(dmgFileName)
    [success, output] = execute(command)
    return success

def addStandardParserParameters(parser):
    parser.add_argument("--user", metavar = "<Apple Developer ID>", dest = "user", required = True)
    parser.add_argument("--password", metavar = "<Apple Developer Password>", dest = "password", required = True)
    parser.add_argument("--timeout", metavar = "<Timeout of opeartion in minutes>", dest = "timeout", type = int, default = 20)
 
def showCriticalErrorAndExit(message):
    sys.stderr.write(message)
    sys.exit(1)

def main():
    startTime = time.time()

    parser = argparse.ArgumentParser(description = "Notarizes specified application")
    
    subparsers = parser.add_subparsers()
    notarizationParser = subparsers.add_parser("notarize")
    addStandardParserParameters(notarizationParser)
    notarizationParser.add_argument("--team-id", metavar = "<Apple Team ID>", dest = "teamId", required = True)
    notarizationParser.add_argument("--file-name", metavar = "<Dmg File Name>", dest = "dmgFileName", required = True)
    notarizationParser.add_argument("--bundle-id", metavar = "<Application Bundle ID>", dest = "bundleId", required = True)
    
    checkParsers = subparsers.add_parser("check")
    addStandardParserParameters(checkParsers)
    checkParsers.add_argument("--request-id", metavar = "<Request ID>", dest = "requestId", required = True)
    
    options = parser.parse_args()
    needUpload = ('requestId' not in options) or (options.requestId is None)
    if needUpload: 
        [options.requestId, uploadError] = uploadForNotarization(options)
    
    if not options.requestId:
        showCriticalErrorAndExit("Can't upload package for notarization, see errors:\n {}\n".format(uploadError))
    
    if needUpload:
        sys.stdout.write("Upload successeful, request id is {}".format(options.requestId))

    kCheckPeriodSeconds = 30
    [success, notarizationError] = waitForNotarizationCompletion(options, kCheckPeriodSeconds)
    
    elapsedTimeMessage = "Total time is {}\n".format(datetime.timedelta(seconds = (time.time() - startTime)))
    if not success:
        showCriticalErrorAndExit("\nNotarization failed:\n{0}\n\n{1}\n".format(notarizationError, elapsedTimeMessage))
    
    sys.stdout.write("\nNotarization successful\n\n{}".format(elapsedTimeMessage))
    if not stapleApp(options.dmgFileName):
        showCriticalErrorAndExit("Can't staple application")
    
main()
