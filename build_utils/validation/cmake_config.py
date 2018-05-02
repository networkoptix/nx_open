import re


def read_cmake_config(filename):
    contents = None
    with open(filename) as file:
        contents = file.read()

    config = {}
    if not contents:
        return config

    pattern = re.compile('^set\((.+?) (.+?)\)', re.DOTALL | re.MULTILINE)
    for k, v in pattern.findall(contents):
        config[k.strip()] = v.strip()

    return config
