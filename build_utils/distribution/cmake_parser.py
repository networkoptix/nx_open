#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

def parse_boolean(value):
    '''
    From CMake doc (https://cmake.org/cmake/help/latest/command/if.html):
    if(<constant>)
    True if the constant is 1, ON, YES, TRUE, Y, or a non-zero number. False if the constant is 0,
    OFF, NO, FALSE, N, IGNORE, NOTFOUND, the empty string, or ends in the suffix -NOTFOUND. Named
    boolean constants are case-insensitive.
    '''
    value = value.upper()
    if value == "ON" or value == "YES" or value == "TRUE" or value == "Y":
        return True
    if value == "OFF" or value == "NO" or value == "FALSE" or value == "N":
        return False
    if not value:
        return False
    if value == "NOTFOUND" or value.endswith("-NOTFOUND"):
        return False
    return int(value) != 0


def _test_parse_boolean():
    assert(parse_boolean("ON"))
    assert(parse_boolean("on"))
    assert(parse_boolean("10"))
    assert(parse_boolean("-1"))
    assert(not parse_boolean(""))
    assert(not parse_boolean("TEST-NOTFOUND"))
    try:
        parse_boolean("something strange")
    except ValueError:
        pass


if __name__ == "__main__":
    _test_parse_boolean()
