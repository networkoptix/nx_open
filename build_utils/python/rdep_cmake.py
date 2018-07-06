from __future__ import print_function
import os
import sys
import posixpath
import argparse
import glob
from rdep import Rdep

class RdepSyncher:
    def __init__(self, rdep_repo_root):
        self.versions = {}
        self.rdep_target = None
        self.prefer_debug_packages = False

        self.rdep = Rdep(rdep_repo_root)
        self.rdep.fast_check = True
        self.rdep.load_timestamps_for_fast_check()

        self._exported_paths = {}
        self._synched_package_dirs = []

    def sync(self, package, path_variable=None, optional=False, use_local=False):
        target, pack = posixpath.split(package)

        self.rdep.targets = [target] if target else [self.rdep_target]

        version = self.versions.get(pack)

        full_package_name = pack + "-" + version if version else pack

        sync_func = self.rdep.locate_package if use_local else self.rdep.sync_package

        package_found = False
        if self.prefer_debug_packages:
            if sync_func(full_package_name + "-debug"):
                package_found = True
                full_package_name += "-debug"

        if not package_found:
            package_found = sync_func(full_package_name)

        if not package_found:
            if not optional:
                print("error: Cannot sync package {}".format(package), file=sys.stderr)
                exit(1)

            return False

        path = self.rdep.locate_package(full_package_name).replace("\\", "/")

        self._synched_package_dirs.append(path)
        if path_variable:
            self._exported_paths[path_variable] = path

        return True

    def generate_cmake_include(self, file_name):
        with open(file_name, "w") as f:
            f.write("# Package versions.\n")
            for k, v in self.versions.iteritems():
                f.write("set({0}_version \"{1}\")\n".format(k, v))

            f.write("\n# Exported package directories.\n")
            for k, v in self._exported_paths.iteritems():
                f.write("set({0} \"{1}\")\n".format(k, v))

            f.write("\n# List of synchronized package directories.\n")
            f.write("set(synched_package_dirs\n")
            for path in self._synched_package_dirs:
                f.write("    \"{}\"\n".format(path))
            f.write(")\n")

            f.write("\n# Includes.\n")
            for path in self._synched_package_dirs:
                cmake_files = glob.glob(os.path.join(path, "*.cmake"))
                if cmake_files and len(cmake_files) == 1:
                    f.write("include(\"{}\")\n".format(cmake_files[0].replace("\\", "/")))
