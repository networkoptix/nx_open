#!/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


import argparse
import yaml


def print_dependencies(variable, deps):
    formatted_dependencies = []
    for dep in deps:
        if isinstance(dep, str):
            formatted_dependencies.append(dep)
        else:
            formatted_dependencies.append(" | ".join(dep))

    print(f'set({variable} "' + ", ".join(formatted_dependencies) + '")')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", help="Dependency list file")
    args = parser.parse_args()

    with open(args.file, "r") as file:
        deps = yaml.load(file, yaml.Loader)

    print_dependencies("required_dependencies", deps.get("depends", {}))
    print_dependencies("recommended_dependencies", deps.get("recommends", {}))


if __name__ == "__main__":
    main()
