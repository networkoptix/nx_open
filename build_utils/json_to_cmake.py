#!/bin/env python


from __future__ import print_function
import argparse
import json


def join_prefix(s1, s2):
    return "%s.%s" % (s1, s2) if s1 else s2


def parse_value(value):
    if type(value) is int:
        return str(value)
    elif type(value) is bool:
        return "TRUE" if value else "FALSE"
    else:
        return '"' + str(value) + '"'


def parse_list(l):
    result = ""
    for value in l:
        result += "\n    " + parse_value(value)
    return result


def parse_dict(d, prefix):
    if type(d) is not dict:
        return None

    result = {}

    for key, value in d.items():
        var = join_prefix(prefix, key)
        if type(value) is dict:
            result.update(parse_dict(value, var))
        elif type(value) is list:
            result[var] = parse_list(value)
        else:
            result[var] = parse_value(value)

    return result


def convert_json_file_to_cmake_file(json_file_name, cmake_file_name, prefix=None):
    with open(json_file_name) as json_file:
        variables = parse_dict(json.load(json_file), prefix=prefix)

    with open(cmake_file_name, "w") as cmake_file:
        if variables:
            for var, value in variables.items():
                cmake_file.write("set({} {})\n".format(var, value))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=str, help="Input JSON file name")
    parser.add_argument("output", type=str, help="Output CMake file name")
    parser.add_argument(
        "--prefix", "-p", type=str, default=None,
        help="Prefix for CMake variables")

    args = parser.parse_args()
    convert_json_file_to_cmake_file(args.input, args.output, prefix=args.prefix)


if __name__ == '__main__':
    main()
