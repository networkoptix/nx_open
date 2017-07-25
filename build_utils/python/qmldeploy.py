#!/usr/bin/env python

from __future__ import print_function
import os
import sys
import argparse
import subprocess
import shutil
import json
import re

class QmlDeployUtil:
    def __init__(self, qt_root):
        self.qt_root = os.path.abspath(qt_root)

        self.scanner_path = self.find_qmlimportscanner(qt_root)
        if not self.scanner_path:
            exit("qmlimportscanner is not found in {}".format(qt_root))
            raise

        self.import_path = self.find_qml_import_path(qt_root)
        if not self.import_path:
            exit("qml import path is not found in {}".format(qt_root))
            raise

        self.modules_path = self.find_modules_path(qt_root)
        if not self.import_path:
            exit("modules path is not found in {}".format(qt_root))
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

    @staticmethod
    def find_modules_path(qt_root):
        path = os.path.join(qt_root, "mkspecs", "modules")
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
                    "plugin": item.get("plugin"),
                    "class_name": item.get("classname")
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

    def get_plugin_information(self, plugin_name):
        pri_file_name = os.path.join(self.modules_path, "qt_plugin_{}.pri".format(plugin_name))

        if not os.path.exists(pri_file_name):
            return

        with open(pri_file_name) as pri_file:
            pri_data = pri_file.read()

        re_prefix = "QT_PLUGIN\\." + plugin_name + "\\."

        m = re.search(re_prefix + "TYPE = (.+)", pri_data)
        if not m:
            return
        plugin_type = m.group(1)

        file_path = os.path.join(self.qt_root, "plugins", plugin_type, "lib" + plugin_name + ".a")
        if not os.path.exists(file_path):
            return

        m = re.search(re_prefix + "CLASS_NAME = (.+)", pri_data)
        if not m:
            return
        plugin_class = m.group(1)

        return {
            "name": plugin_name,
            "path": file_path,
            "class_name": plugin_class
        }

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

    def print_static_plugins(self, qml_root, file_name=None, additional_plugins=[]):
        imports_dict = self.invoke_qmlimportscanner(qml_root)
        if not imports_dict:
            return

        imports = self.get_qt_imports(imports_dict)

        if file_name:
            output = open(file_name, "w")
        else:
            output = sys.stdout

        for item in imports:
            path = item["path"]
            plugin = item["plugin"]

            if not plugin:
                continue

            name = os.path.join(path, "lib" + plugin + ".a")
            if os.path.exists(name):
                output.write(name + "\n")

        for plugin_name in additional_plugins:
            info = self.get_plugin_information(plugin_name)
            if not info:
                continue

            output.write(info["path"] + "\n")

        if output != sys.stdout:
            output.close()

    def generate_import_cpp(self, qml_root, file_name, additional_plugins=[]):
        imports_dict = self.invoke_qmlimportscanner(qml_root)
        if not imports_dict:
            return

        imports = self.get_qt_imports(imports_dict)

        with open(file_name, "w") as out_file:
            out_file.write("// This file is generated by qmldeploy.py\n")
            out_file.write("#include <QtPlugin>\n\n")

            for item in imports:
                class_name = item["class_name"]
                if not class_name:
                    continue

                out_file.write("Q_IMPORT_PLUGIN({})\n".format(class_name))

            for plugin_name in additional_plugins:
                info = self.get_plugin_information(plugin_name)
                if not info:
                    continue

                out_file.write("Q_IMPORT_PLUGIN({})\n".format(info["class_name"]))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qmlimportscanner", type=str, help="Custom qmlimportscanner binary.")
    parser.add_argument("--qml-root", type=str, required=True, help="Root QML directory.")
    parser.add_argument("--qt-root", type=str, required=True, help="Qt root directory.")
    parser.add_argument("-o", "--output", type=str, help="Output.")
    parser.add_argument("--print-static-plugins", action="store_true", help="Print static plugins list.")
    parser.add_argument("--generate-import-cpp", action="store_true", help="Generate a file for importing plugins.")
    parser.add_argument("--additional-plugins", nargs="+", help="Additional plugins to process.")

    args = parser.parse_args()

    deploy_util = QmlDeployUtil(args.qt_root)
    if args.qmlimportscanner:
        if not os.path.exists(args.qmlimportscanner):
            exit("{}: not found".format(args.qmlimportscanner))
        deploy_util.scanner_path = args.qmlimportscanner

    if args.print_static_plugins:
        deploy_util.print_static_plugins(args.qml_root, args.output, args.additional_plugins)
    elif args.generate_import_cpp:
        deploy_util.generate_import_cpp(args.qml_root, args.output, args.additional_plugins)
    elif args.output:
        if not arg.output:
            exit("Output directory is not specified.")

        deploy_util.deploy(args.qml_root, args.output)

if __name__ == "__main__":
    main()
