from os import system
import json
import codecs

with codecs.open("language_list.json", 'r', encoding='utf-8-sig') as languages_list:
        langList = json.load(languages_list)

def allLanguages():
    for name in langList:
        system('robot -N ' + name + ' -d outputs -e not-ready -V getvars.py:' + langList[name] + ' --timestampoutputs test-cases')

if __name__ == "__main__":
    allLanguages()