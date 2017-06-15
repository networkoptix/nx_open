import os
import ConfigParser
import time

class ConfigHelper:
    FILE_NAME = None

    def __init__(self, path):
        if os.path.isdir(path):
            self.__file_name = os.path.join(path, self.FILE_NAME)
        else:
            self.__file_name = path
        self.__config = ConfigParser.ConfigParser()
        try:
            self.__config.read(self.__file_name)
        except:
            pass

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


class RdepConfig(ConfigHelper):
    FILE_NAME = ".rdeprc"

    def __init__(self):
        ConfigHelper.__init__(self, os.path.join(os.path.expanduser("~")))
        self.add_section("General")

    def get_name(self):
        return self.get_value("General", "name", "").strip()
    def set_name(self, name):
        self.set_value("General", "name", name)

    def get_ssh(self):
        return self.get_value("General", "ssh")
    def set_ssh(self, ssh):
        self.set_value("General", "ssh", ssh)

    def get_rsync(self, default_value = None):
        return self.get_value("General", "rsync", default_value)
    def set_rsync(self, rsync):
        self.set_value("General", "rsync", rsync)

class RepositoryConfig(ConfigHelper):
    FILE_NAME = ".rdep"

    def __init__(self, file_name):
        ConfigHelper.__init__(self, file_name)

    def get_url(self):
        return self.get_value("General", "url")
    def set_url(self, url):
        self.set_value("General", "url", url)

    def get_push_url(self, default_value = None):
        return self.get_value("General", "push_url", default_value)
    def set_push_url(self, push_url):
        self.set_value("General", "push_url", push_url)

    def get_ssh(self):
        return self.get_value("General", "ssh")
    def set_ssh(self, ssh):
        return self.set_value("General", "ssh", ssh)

class PackageConfig(ConfigHelper):
    FILE_NAME = ".rdpack"

    def __init__(self, file_name):
        ConfigHelper.__init__(self, file_name)

    def get_timestamp(self, default_value = None):
        timestamp = self.get_value("General", "time", default_value)
        return None if timestamp == None else int(timestamp)
    def update_timestamp(self):
        self.set_value("General", "time", int(time.time()))

    def get_uploader(self):
        return self.get_value("General", "uploader")
    def set_uploader(self, name):
        self.set_value("General", "uploader", name)

    def get_copy_list(self):
        if not self.has_section("Copy"):
            return { "bin": [ "bin/*" ], "lib": [ "lib/*.so*", "lib/*.dylib" ] }
        result = {}
        for key, value in self.get_items("Copy"):
            result[key] = value.split()
        return result
