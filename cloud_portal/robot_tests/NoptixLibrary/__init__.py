#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os.path
import time
import types
from robot.utils import NormalizedDict
from selenium import webdriver
from selenium.webdriver.common.keys import Keys

from selenium.common.exceptions import NoSuchElementException
from SeleniumLibrary.base import keyword, LibraryComponent
from SeleniumLibrary.locators import WindowManager
from SeleniumLibrary.utils import (is_falsy, is_truthy, secs_to_timestr,
                                   timestr_to_secs, SELENIUM_VERSION)
from random import *
from robot.libraries.BuiltIn import BuiltIn


class NoptixLibrary(object):


    def copy_text(self, locator):
        locator.send_keys(Keys.CONTROL+'a')
        locator.send_keys(Keys.CONTROL+'c')
        

    def paste_text(self, locator):
        locator.send_keys(Keys.CONTROL+'v')


    def get_random_email(self):
        return "noptixqa+" + str(time.time()) + "@gmail.com"

    def get_many_random_emails(self, howMany):
        emails = []
        for x in xrange(0,int(howMany)):
            emails.append(self.get_random_email())
            time.sleep(.2)
        return emails

    def get_random_symbol_email(self):
        return '''!#$%&'*+-/=?^_`{|}~''' + str(time.time()) + "@gmail.com"


    def wait_until_textfield_contains(self, locator, expected, timeout=5):
        seleniumlib = BuiltIn().get_library_instance('Selenium2Library')
        timeout = timeout + time.time()
        not_found = None
        
        while time.time() < timeout:
            try:
                element = seleniumlib.find_element(locator)
                value = element.get_attribute('value')
                if value == expected:
                    return
            except:
                not_found = "Something weird happened it's Kyle's fault"
            else:
                not_found = None
            time.sleep(.2)  
        raise AssertionError(not_found)


    def check_online_or_offline(self, elements):
        for element in elements:
            try:
                if element.find_element_by_xpath(".//button[@ng-click='checkForm()']"): 
                    print "online"
            except NoSuchElementException:
                try:
                    if element.find_element_by_xpath(".//span[contains(text(),'offline')]"):                    
                        print "offline"
                except: raise NoSuchElementException