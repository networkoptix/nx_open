#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import os
import subprocess
import xml.etree.ElementTree as ET


def parse():
    output = subprocess.check_output(
        ["/usr/libexec/PlistBuddy", "-x", "-c", "print IDEProvisioningTeams",
         f"{os.environ['HOME']}/Library/Preferences/com.apple.dt.Xcode.plist"])

    for e in ET.fromstring(output).findall('dict/array/dict'):
        team = {}

        items = list(e.iter())

        for i, item in enumerate(items):
            if item.tag == "key":
                nextItem = items[i + 1]

                if nextItem.tag == 'true':
                    team[item.text] = True
                elif nextItem.tag == 'false':
                    team[item.text] = False
                elif nextItem.tag == 'string':
                    team[item.text] = nextItem.text

        yield team


def isfree(args):
    isFreeProvisioningTeam = \
        next(t for t in parse() if t.get('teamID') == args.id).get('isFreeProvisioningTeam')
    print(isFreeProvisioningTeam)


def select(args):
    teams = list(parse())
    team = next((t for t in teams if not t.get('isFreeProvisioningTeam')), teams[0])
    print(team['teamID'])


def listTeams(args):
    for team in parse():
        print(team['teamID'], team['teamName'])


def setup_arguments_parser():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(required=True)

    parser_free = subparsers.add_parser(
        "isfree", help="Check if provided team id is free")
    parser_free.set_defaults(func=isfree)
    parser_free.add_argument("id", help="Apple Team ID")

    parser_select = subparsers.add_parser("select",
        help=("Select first non-free team id. If no non-free team id is found, "
              "select the first encountered free team id."))
    parser_select.set_defaults(func=select)

    parser_list = subparsers.add_parser(
        "list", help="List available team ids")
    parser_list.set_defaults(func=listTeams)

    return parser


def main():
    parser = setup_arguments_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
