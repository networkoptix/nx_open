#!/bin/env python

from __future__ import print_function
import argparse

try:
    # For Python 2.
    import ConfigParser as configparser
except:
    # For Python 3.
    import configparser

parser = argparse.ArgumentParser()
parser.add_argument("config_file", help="Configuration file name")
parser.add_argument("key", default=None, nargs="?", help="A key to read, write or delete")
parser.add_argument("value", default=None, nargs="?",
    help="When specified, this value will be assigned to the specified key")
parser.add_argument("--delete", "-d", action="store_true", help="Delete the specified key")

args = parser.parse_args()

config = configparser.ConfigParser()
config.optionxform = str
config.read(args.config_file)

has_changes = False

if args.key:
    if args.delete:
        if config.has_section("General") and config.has_option("General", args.key):
            config.remove_option("General", args.key)
            has_changes = True
    elif args.value:
        if not config.has_section("General"):
            config.add_section("General")
        config.set("General", args.key, args.value)
        has_changes = True
    else:
        try:
            print(config.get("General", args.key))
        except configparser.NoSectionError:
            pass
        except configparser.NoOptionError:
            pass
else:
    try:
        for name, value in config.items("General"):
            print("{}='{}'".format(name, value))
    except configparser.NoSectionError:
        pass

if has_changes:
    with open(args.config_file, "w") as f:
        config.write(f)
