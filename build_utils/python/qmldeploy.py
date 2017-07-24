#!/usr/bin/env python

from __future__ import print_function
import os
import sys
import argparse
import subprocess
import shutil
import json

class QmlDeployUtil:
    def __init__(self, qt_root):
        self.qt_root = os.path.abspath(qt_root)

        self.scanner_path = QmlDeployUtil.find_qmlimportscanner(qt_root)
        if not self.scanner_path:
            exit("qmlimportscanner is not found in {}".format(qt_root))
            raise

        self.import_path = QmlDeployUtil.find_qml_import_path(qt_root)
        if not self.import_path:
            exit("qml import path is not found in {}".format(qt_root))
            raise

    @staticmethod
    def find_qmlimportscanner(qt_root):
        for name in ["qmlimportscanner", "qmlimportscanner.exe"]:
            path = os.path.join(qt_root, "bin", name)

            if os.path.exists(path):
                return path

        return None

    @staticmethod
    def find_qml_import_path(qt_root):
        path = os.path.join(qt_root, "qml")
        return path if os.path.exists(path) else None

    def invoke_qmlimportscanner(self, qml_root):
        command = [self.scanner_path, "-rootPath", qml_root, "-importPath", self.import_path]
        process = subprocess.Popen(command, stdout=subprocess.PIPE)

        if not process:
            exit("Cannot start {}".format(" ".join(command)))
            return

        return json.load(process.stdout)

    def get_qt_imports(self, imports):
        if not type(imports) is list:
            exit("Parsed imports is not a list.")

        qt_deps = []

        for item in imports:
            path = item.get("path")

            if not path or os.path.commonprefix([self.qt_root, path]) != self.qt_root:
                continue

            qt_deps.append(
                {
                    "path": path,
                    "plugin": item.get("plugin")
                })

        qt_deps.sort(key=lambda item: item["path"])

        result = []
        previous_path = None

        for item in qt_deps:
            path = item["path"]
            if previous_path and path == previous_path:
                continue

            result.append(item)
            previous_path = path

        return result

    def copy_components(self, imports, output_dir):
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        paths  = []
        previous_path = None

        for item in imports:
            path = item["path"]
            if previous_path and path.startswith(previous_path):
                continue

            paths.append(path)
            previous_path = path

        for path in paths:
            subdir = os.path.relpath(path, self.import_path)
            dst = os.path.join(output_dir, subdir)
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.copytree(path, dst, symlinks=True,
                ignore = shutil.ignore_patterns("*.a", "*.prl"))

    def deploy(self, qml_root, output_dir):
        imports_dict = self.invoke_qmlimportscanner(qml_root)
        if not imports_dict:
            return

        imports = self.get_qt_imports(imports_dict)
        self.copy_components(imports, output_dir)

    def print_static_plugins(self, qml_root):
        imports_dict = self.invoke_qmlimportscanner(qml_root)
        if not imports_dict:
            return

        imports = self.get_qt_imports(imports_dict)

        for item in imports:
            path = item["path"]
            plugin = item["plugin"]

            if not plugin:
                continue

            name = os.path.join(path, "lib" + plugin + ".a")
            if os.path.exists(name):
                print(name)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qml-root", type=str, required=True, help="Root QML directory.")
    parser.add_argument("--qt-root", type=str, required=True, help="Qt root directory.")
    parser.add_argument("-o", "--output", type=str, help="Output directory.")
    parser.add_argument("--print-static-plugins", action="store_true", help="Print static plugins list.")

    args = parser.parse_args()

    deploy_util = QmlDeployUtil(args.qt_root)

    if args.print_static_plugins:
        deploy_util.print_static_plugins(args.qml_root)
    else:
        if not args.output:
            exit("Output directory is not specified.")

        deploy_util.deploy(args.qml_root, args.output)

if __name__ == "__main__":
    main()
