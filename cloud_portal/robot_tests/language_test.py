from os import system, path, remove
import json
import codecs
import time
from datetime import datetime
from check_server import ping

waitTime = 30

with codecs.open('language_list.json', 'r', encoding='utf-8-sig') as languages_list:
    langList = json.load(languages_list)


def allLanguages():

    for key in langList:
        while True:
            if runTest(key, langList):
                rebotString = ""
                for lang in langList:
                    if path.isfile(path.join('outputs', lang, 'output.xml ')):
                        rebotString += path.join('outputs',
                                                 lang, 'output.xml ')
                system(
                    'rebot -o allLanguages.xml -l allLanguagesLog.html -r allLanguagesReport.html ' + rebotString)
                break


def runTest(key, langList):

    print datetime.now(), 'Server status:', ping().status_code
    while True:
        # ping the server at the start to make sure it's ready
        if ping().ok:
            system('robot -N {}{}{} -d {} -e not-ready -V getvars.py:{} test-cases'.format(
                langList[key], "_", key, path.join('outputs', key), key))
            # ping the server at the end to make sure it didn't go down
            if ping().ok:
                return True
            else:
                # if server went down in the middle, discard the results and try
                # again when server is up
                discardLanguageResult(key)
                time.sleep(waitTime)
        else:
            time.sleep(waitTime)


def discardLanguageResult(key):

    remove(path.join('outputs', key, 'output.xml'))
    remove(path.join('outputs', key, 'log.html'))
    remove(path.join('outputs', key, 'report.html'))


if __name__ == '__main__':
    allLanguages()
