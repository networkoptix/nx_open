#!/bin/python2

import argparse
import yaml

from windeployqt_interface import deploy_qt, cleanup_qtwebprocess

def collect_qml_dependencies(qt_directory, source, qml_root, output):
    deploy_qt(qt_directory, source, qml_root, output)
    cleanup_qtwebprocess(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', help="Config file", required=True)
    parser.add_argument('--source', help="Source binary", required=True)
    parser.add_argument('--qml-root', help="Source binary", required=True)
    parser.add_argument('--output', help="Output directory", required=True)
    args = parser.parse_args()

    with open(args.config, 'r') as f:
        config = yaml.load(f)

    collect_qml_dependencies(config['qt_directory'], args.source, args.qml_root, args.output)


if __name__ == '__main__':
    main()
