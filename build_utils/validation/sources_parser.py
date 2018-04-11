#!/bin/python2
# -*- coding: utf-8 -*-

import os
import re
import pyfscache

from file_formats import is_image_file, is_source_file

sources_cache = pyfscache.FSCache('.sources_cache', hours=1, minutes=2.5)


def parse_line(line, extension, location):
    parts = re.split("[^a-z0-9_/@%.]", line, flags=re.IGNORECASE)
    for part in parts:
        if not is_image_file(part):
            continue
        if part.startswith('/'):
            part = part[1:]
        if part.startswith('skin/'):
            part = part[5:]
        yield part, location


def parse_file(path):
    linenumber = 0
    extension = os.path.splitext(path)[1]
    with open(path, "r") as file:
        for line in file.readlines():
            linenumber += 1
            for img, location in parse_line(line, extension, "%s:%s" % (path, linenumber)):
                yield img, location


@sources_cache
def parse_sources_cached(dir):
    result = []
    for line, location in parse_sources(dir):
        result.append((line, location))
    return result


def clear_sources_cache():
    sources_cache.purge()


def parse_sources(dir):
    if not os.path.exists(dir):
        return
    for entry in os.listdir(dir):
        path = os.path.join(dir, entry)
        if (os.path.isdir(path)):
            for line, location in parse_sources(path):
                yield line, location
            continue
        if not is_source_file(path):
            continue
        for line, location in parse_file(path):
            yield line, location


if __name__ == "__main__":
    for line, location in parse_sources_cached(os.getcwd()):
        print "{0} at {1}".format(line, location)
