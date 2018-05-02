from os import system, name, path
import json
import codecs
import time
from datetime import datetime
from check_server import ping

waitTime = 300

with codecs.open('language_list.json', 'r', encoding='utf-8-sig') as languages_list:
        langList = json.load(languages_list)


def allLanguages():


    for name in langList:
        print datetime.now()
        print 'Server status:', ping().status_code
        while True:
            if runTest(name, langList):
                rebotString = ""
                for lang in langList:
                    if os.path.isfile(path.join('outputs', langList[lang], 'outputs.xml ')):
                        rebotString += path.join('outputs', langList[lang], 'outputs.xml ')
                system ('rebot -o allLanguages.xml -l allLanguagesLog.html -r allLanguagesReport.html ' + rebotString)
                break
        

def runTest(name, langList):


    if ping().ok:
        system('robot -N ' + name + ' -v headless:true -d ' + path.join('outputs', langList[name]) + ' -e not-ready -V getvars.py:' + langList[name] + ' test-cases')
        if ping().ok:
            return True
        else:
            removeBadOutputs(name, langList)
            while not ping().ok:
                time.sleep(waitTime)
            return runTest(name, langList)
    else:
        while not ping().ok:
            time.sleep(waitTime)
        return runTest(name, langList)


def removeBadOutputs(name, langList):


    remove(path.join('outputs', langList[name], 'output.xml'))
    remove(path.join('outputs', langList[name], 'log.html'))
    remove(path.join('outputs', langList[name], 'report.html'))


if __name__ == '__main__':
    allLanguages()