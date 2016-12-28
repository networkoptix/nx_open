#!/usr/bin/env python

import os
import sys
import subprocess
import ConfigParser

QBS = os.environ["QBS"]
if not QBS:
    QBS = "qbs"

class ConfigHelper:
    FILE_NAME = None

    def __init__(self, path):
        if os.path.isdir(path):
            self.__file_name = os.path.join(path, self.FILE_NAME)
        else:
            self.__file_name = path
        self.__config = ConfigParser.ConfigParser()
        if os.path.isfile(self.__file_name):
            self.__config.read(self.__file_name)

    def get_file_name(self):
        return self.__file_name

    def has_section(self, section):
        return self.__config.has_section(section)

    def add_section(self, section):
        if not self.__config.has_section(section):
            self.__config.add_section(section)

        with open(self.__file_name, "w") as file:
            self.__config.write(file)

    def get_items(self, section):
        return self.__config.items(section)

    def get_value(self, section, option, default_value = None):
        if not self.__config.has_option(section, option):
            return default_value

        return self.__config.get(section, option)

    def set_value(self, section, option, value):
        if not self.__config.has_section(section):
            self.__config.add_section(section)

        self.__config.set(section, option, value)
        with open(self.__file_name, "w") as file:
            self.__config.write(file)

    def remove_value(self, section, option):
        if not self.__config.has_section(section):
            return

        self.__config.remove_option(section, option)
        with open(self.__file_name, "w") as file:
            self.__config.write(file)

class QbsConfig(ConfigHelper):
    FILE_NAME = "qbs.ini"

    def __init__(self, path):
        ConfigHelper.__init__(self, path)

        self.project = self.get_value("General", "project")
        if not self.project:
            print >> sys.stderr, "project is not defined in {}".format(self.get_file_name())
            return

        if not os.path.exists(self.project):
            print >> sys.stderr, "project path does not exist {}".format(self.project)
            self.project = None
            return

    def get_properties(self):
        result = {}
        for (k, v) in self.get_items("Properties"):
            result[k] = v
        return result

    def set_property(self, name, value):
        if value != None:
            self.set_value("Properties", name, value)
        else:
            self.remove_value("Properties", name)

def main():
    path = os.getcwd()
    config = QbsConfig(path)
    if not config.project:
        exit(1)

    possible_actions = ["build", "install", "resolve", "set", "get"]
    action = "build"
    argv_props = sys.argv[1:]

    if len(sys.argv) > 1 and sys.argv[1] in possible_actions:
        action = sys.argv[1]
        argv_props = sys.argv[2:]

    if action == "get":
        props = config.get_properties()

        if len(sys.argv) > 2:
            for prop in argv_props:
                print "{}:{}".format(prop, props.get(prop, None))
        else:
            profile = props.pop("profile")
            if profile:
                print "profile:{}".format(profile),

            for (k, v) in props.items():
                print "{}:{}".format(k, v),

        return

    if action == "set":
        for prop in argv_props:
            (name, _, value) = prop.partition(":")
            if value == "":
                value = None
            config.set_property(name, value)

        return

    if action == "build" or action == "install" or action == "resolve":
        props = config.get_properties()

        profile = props.pop("profile", None)

        for prop in argv_props:
            (name, _, value) = prop.partition(":")
            if value != "":
                props[name] = value

        command = [QBS, action]

        command.append("-f")
        command.append(config.project)

        if profile:
            command.append("profile:" + profile)

        for (k, v) in props.items():
            command.append("{}:{}".format(k, v))

        if subprocess.call(command) != 0:
            exit(1)

if __name__ == "__main__":
    main()
