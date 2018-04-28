from os import system
import json
import codecs
import time
from check_server import ping

with codecs.open("language_list.json", 'r', encoding='utf-8-sig') as languages_list:
        langList = json.load(languages_list)

def allLanguages():
    for name in langList:
        print ping().status_code
        if ping().ok:
            system('robot -N ' + name + ' -v headless:true -d outputs\\' + langList[name] + ' -e not-ready -V getvars.py:' + langList[name] + ' test-cases')
        else:
            while not ping().ok:
                time.sleep(300)
                if ping().ok:
                    system('robot -N ' + name + ' -v headless:true -d outputs\\' + langList[name] + ' -e not-ready -V getvars.py:' + langList[name] + ' test-cases')
                    break
        


if __name__ == "__main__":
    allLanguages()