#!/usr/bin/env python
# -*- coding: utf-8 -*-

import imaplib
import re
from platform import system
import email.header
from email.parser import HeaderParser
import os.path
import time
import types
from requests import head
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
        locator.send_keys(Keys.CONTROL + 'a')
        locator.send_keys(Keys.CONTROL + 'c')

    def paste_text(self, locator):
        locator.send_keys(Keys.CONTROL + 'v')

    def get_random_email(self, email):
        index = email.find('@')
        email = email[:index] + '+' + str(time.time()) + email[index:]
        return email

    def get_many_random_emails(self, howMany, email):
        emails = []
        for x in xrange(0, int(howMany)):
            emails.append(self.get_random_email())
            time.sleep(.2)
        return emails

    def get_random_symbol_email(self, email):
        index = email.find('@')
        email = email[:index] + \
            "+!#$%'*-/=?^_`{|}~" + str(time.time()) + email[index:]
        return email

    def wait_until_textfield_contains(self, locator, expected, timeout=10):
        seleniumlib = BuiltIn().get_library_instance('SeleniumLibrary')
        timeout = timeout + time.time()
        not_found = None

        while time.time() < timeout:
            try:
                element = seleniumlib.find_element(locator)
                value = element.get_attribute('value')
                if value == expected:
                    return
            except:
                pass
            time.sleep(.2)
        raise Exception("No element found with text " + expected)

    def wait_until_element_has_style(self, locator, styleAttribute, expected, timeout=10):
        seleniumlib = BuiltIn().get_library_instance('SeleniumLibrary')
        timeout = timeout + time.time()
        not_found = None

        while time.time() < timeout:
            try:
                element = seleniumlib.find_element(locator)
                value = element.value_of_css_property(styleAttribute)
                if value == expected:
                    return
            except:
                not_found = "No element found with style " + expected
            time.sleep(.2)
        raise AssertionError(not_found)

    def check_online_or_offline(self, elements, offlineText):
        for element in elements:
            try:
                if element.find_element_by_xpath(".//button[@ng-click='checkForm()']"):
                    print("online")
            except NoSuchElementException:
                try:
                    if element.find_element_by_xpath(".//span[contains(text(),'" + offlineText + "')]"):
                        print("offline")
                except:
                    raise NoSuchElementException

    ''' Get the email subject from an email in other languages

    Takes the email UID and the expected text of the email subject.
    The important part of this is in the for loop.  First we take the 2nd item
    in the subject, which is the actual subject text, and decode it from ascii
    so that it is not a byte string.  Then use decode_header to decode the
    base64 string into unicode bytes.  Then finally we take that string and
    decode with UTF-8 to get the actual text.  Note that in some cases decoding
    UTF-8 is unnecessary (english, spanish, etc.) so the else statement clears 
    any remaining junk.  Here is the process:
    
    1.  Initial value:  b'Subject: =?utf-8?b?INCQ0LrRgtC40LLQuNGA0YPQudGC0LUg0YPRh9C10YLQvdGD0Y4g0LfQsNC/?=\r\n =?utf-8?b?0LjRgdGM?=\r\n\r\n'
    2.  Decoded ASCII:  Subject: =?utf-8?b?INCQ0LrRgtC40LLQuNGA0YPQudGC0LUg0YPRh9C10YLQvdGD0Y4g0LfQsNC/?= =?utf-8?b?0LjRgdGM?=
    3.  Decoded header: [(b'Subject: ', None), (b' \xd0\x90\xd0\xba\xd1\x82\xd0\xb8\xd0\xb2\xd0\xb8\xd1\x80\xd1\x83\xd0\xb9\xd1\x82\xd0\xb5 \xd1\x83\xd1\x87\xd0\xb5\xd1\x82\xd0\xbd\xd1\x83\xd1\x8e \xd0\xb7\xd0\xb0\xd0\xbf\xd0\xb8\xd1\x81\xd1\x8c', 'utf-8')]
    4.  Decoded UTF-8 and subbed:  Активируйте учетную запись
    '''

    def check_email_subject(self, email_id, sub_text, email_address, password, host, port):
        conn = imaplib.IMAP4_SSL(host, int(port))
        conn.login(email_address, password)
        conn.select()
        typ, data = conn.uid(
            'fetch', email_id, '(BODY.PEEK[HEADER.FIELDS (SUBJECT)])')
        for res in data:
            if isinstance(res, tuple):
                # Decoding ascii and header
                header = email.header.decode_header(
                    res[1].decode('ascii').strip())
                # Decoding utf-8
                header_str = "".join([x[0].decode(
                    'utf-8').strip() if x[1] else re.sub("(^b\'|\')", "", str(x[0])) for x in header])
                # Removing the word "Subject:" from the string
                header_str = re.sub("Subject:", "", header_str)
                if sub_text != header_str.strip():
                    raise Exception(header_str + ' was not ' + sub_text)
        conn.logout()

    def get_browser_log(self):
        seleniumlib = BuiltIn().get_library_instance('SeleniumLibrary')
        return seleniumlib.driver.get_log('browser')

    def check_file_exists(self, url):
        linkInfo = head(url)
        if int(linkInfo.status_code) == 200: #and int(linkInfo.headers['Content-Length']) > 1000:
            return
        else:
            raise Exception("File does not appear to be available.")

    def check_in_list(self, expected, found):
        for url in expected:
            if found in url:
                return
            elif re.search(url, found):
                return
        raise Exception(url + " was not in the email.")

    def get_os(self):
        plat = system()
        if plat == "Windows":
            return "Windows"
        elif plat == "Darwin":
            return "MacOS"
        elif plat == "Linux":
            return "Linux"
        else:
            raise Exception("Mismatched platform")

    def check_email_button(self, body, env, color):
        pat = '(<a class="btn" href="{})(.[^>]*)(background-color: {};)'.format(
            env, color)
        if re.search(pat, body) == None:
            raise Exception("Button background-color was not found.")

    def check_email_user_names(self, body, fName, lName):
        pat = '(<h1.*>).*({} {}</h1>)'.format(fName, lName)
        if re.search(pat, body) == None:
            raise Exception("User name was not in the email.")

    def check_email_cloud_name(self, body, cloudName):
        pat = '(<p).*({}).*(</p>)'.format(cloudName)
        if re.search(pat, body) == None:
            raise Exception("Cloud name was not in the email.")

    def check_for_blank_target(self, body, url):
        pat = '(<a class="btn" href="{})(.[^>]*)(target=_blank)'.format(url)
        if re.search(pat, body) == None:
            raise Exception("Button target was not 'blank'.")
