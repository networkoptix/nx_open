from os import system, path, remove, listdir
import json
import codecs
import time
from datetime import datetime
from check_server import ping
import sys

waitTime = 30

# Path to the results folder
loc = path.join('C:\\', 'Users', 'Kyle', 'Desktop', 'robot-outputs')
# Path I use on my VM for a shared file
#loc = path.join('/media', 'sf_robot-outputs')

# Get the list of all langauges, updating the JSON file automatically adds
# or removes languages
with codecs.open('language_list.json', 'r', encoding='utf-8-sig') as languages_list:
    langList = json.load(languages_list)


def allLanguages():

    for key in langList:
        runTest(key, langList)
        mergableOutputs = (path.join(loc, lang, 'output.xml')
                           for lang in langList if path.isfile(path.join(loc, lang, 'output.xml')))
        system('rebot --suitestatlevel 2 -d {} -o allLanguages.xml -l allLanguagesLog.html -r allLanguagesReport.html '.format(
            path.join(loc, 'combined-results')) + ' '.join(mergableOutputs))


def runTest(key, langList):

    print datetime.now(), 'Server status:', ping().status_code
    while True:
        # ping the server at the start to make sure it's ready
        if ping().ok:
            # If the server is up, delete screenshots from previous run and
            # start testing.
            map(remove, (path.join(loc, 'combined-results', file) for file in listdir(path.join(loc, 'combined-results'))
                         if file.endswith('.png') and file.find(key) > -1))
            system('robot -N {}_{} -v SCREENSHOTDIRECTORY:{} -v BROWSER:headlesschrome -d {} -e not-ready -V getvars.py:{} test-cases'.format(
                langList[key], key, path.join(loc, 'combined-results'), path.join(loc, key), key))
            # ping the server at the end to make sure it didn't go down
            if ping().ok:
                break
            else:
                # if server went down in the middle, discard the results and try
                # again when server is up
                discardLanguageResult(key)
        time.sleep(waitTime)


def discardLanguageResult(key):

    remove(path.join(loc, key, 'output.xml'))
    remove(path.join(loc, key, 'log.html'))
    remove(path.join(loc, key, 'report.html'))


if __name__ == '__main__':
    allLanguages()
