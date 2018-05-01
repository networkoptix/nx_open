from os import system
import json
import codecs
import time
from datetime import datetime
from check_server import ping

waitTime = 300

with codecs.open("language_list.json", 'r', encoding='utf-8-sig') as languages_list:
        langList = json.load(languages_list)


def allLanguages():


    for name in langList:
        print datetime.now()
        print "Server status:", ping().status_code
        while True:
            if runTest(name, langList):
                #input merging current output files to parent file here
                break
        

def runTest(name, langList):


    if ping().ok:
        system('robot -N ' + name + ' -v headless:true -d outputs\\' + langList[name] + ' -e not-ready -V getvars.py:' + langList[name] + ' test-cases')
        if ping().ok:
            return True
        else:
            #insert removing current output files here
            while not ping().ok:
                time.sleep(waitTime)
            return runTest(name, langList)
    else:
        while not ping().ok:
            time.sleep(waitTime)
            print "loop"
        return runTest(name, langList)


if __name__ == "__main__":
    allLanguages()