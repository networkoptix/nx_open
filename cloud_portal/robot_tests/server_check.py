"""Listener that stops execution if a test fails."""
from os import system
import json
import codecs

import signal
from robot.running import EXECUTION_CONTEXTS
from robot.running import Keyword
import thread
import os

ROBOT_LISTENER_API_VERSION = 3

noFailure = True

with codecs.open("language_list.json", 'r', encoding='utf-8-sig') as languages_list:
        langList = json.load(languages_list)

def end_test(data, result):
    if not result.passed:
        #if server is offline:
        Keyword('fatal error', ()).run(EXECUTION_CONTEXTS.current)

def end_suite(data, result):
    if not result.passed:
        print result.parent, "<----------------------"
        print 'Test "%s" failed: %s' % (result.tags, data.tags)
        if result.message == "Element with locator '//span[@ng-if='registerForm.registerEmail.$touched && registerForm.registerEmail.$error.email' and contains(text(),'Please enter a valid Email')]' not found.":
            print "it worked"
            noFailure = False
        raw_input('Press enter to continue.')
        print noFailure

def close():
    system("robot -i server-down --rerunfailed output.xml --output rerun.xml test-cases\\register-form-validation.robot")
    system("rebot --merge output.xml rerun.xml")






'''
def end_test(data, result):
    if not result.passed:
        if server is down,INSERT API CHECK TO THE SERVER HERE:
            delete output file
            cancel current execution
            while server is down, INSERT API CHECK TO THE SERVER HERE:
                sleep(300)
            re-run previous language
'''