#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os.path
import time
import types

from robot.utils import NormalizedDict
from selenium import webdriver
from selenium.webdriver.common.keys import Keys

from SeleniumLibrary.base import keyword, LibraryComponent
from SeleniumLibrary.locators import WindowManager
from SeleniumLibrary.utils import (is_falsy, is_truthy, secs_to_timestr,
                                   timestr_to_secs, SELENIUM_VERSION)

from random import *

class NoptixLibrary(object):

    def copy_text(self, locator):
        locator.send_keys(Keys.CONTROL+'a')
        locator.send_keys(Keys.CONTROL+'c')

    def paste_text(self, locator):
        locator.send_keys(Keys.CONTROL+'v')

    def get_random_email(self):
        return "noptixqa+" + str(time.time()) + "@gmail.com"

    def  get_random_symbol_email(self):
        return '''!#$%&'*+-/=?^_`{|}~''' + str(time.time()) + "@gmail.com"