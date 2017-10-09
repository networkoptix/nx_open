#!/usr/bin/env python3

import sys
import configparser
import json
import base64

if not (3 <= len(sys.argv) <= 5):
    print ("""Usage: 
            config_helper <file> <base64_json_data>
            config_helper <file> <section> <name>
            config_helper <file> <section> <name> <value>""")
    sys.exit(1)

class ConfigHelper(object):
    def __init__(self, config_file):
        self.config_file = config_file
        self.cfgparser = configparser.ConfigParser()
        self.cfgparser.optionxform = str

        self.read()

    def get(self, section, name):
        return self.cfgparser.get(section, name)

    def set(self, section, name, value):
        if not self.cfgparser.has_section(section):
            self.cfgparser.add_section(section)

        self.cfgparser.set(section, name, value)

    def patch(self, dict):
        for section, section_content in dict.items():
            for name, value in section_content.items():
                self.set(section, name, value)

        self.write()

    def read(self):
        self.cfgparser.read(self.config_file)

    def write(self):
        with open(self.config_file, 'w') as f:
            self.cfgparser.write(f)


def base64decode(data, encoding='utf-8'):
    return base64.b64decode(data.encode(encoding)).decode(encoding)


config_helper = ConfigHelper(sys.argv[1])
if len(sys.argv) == 3:
    b64_json = sys.argv[2]

    json_text = base64decode(b64_json)
    config_dict = json.loads(json_text)

    config_helper.patch(config_dict)

elif len(sys.argv) == 4:
    section, name = sys.argv[2:]

    value = config_helper.get(section_name)
    try:
        print (value)
    except configparser.NoSectionError:
        pass
    except configparser.NoOptionError:
        pass
else:
    section, name, value = sys.argv[2:]

    config_helper.set(section, name, value)
    config_helper.write()

