from __future__ import print_function
import os
import sys
import posixpath
import argparse
import glob
from pathlib import Path

from rdep import Rdep

class RdepSyncher:
    def __init__(self, rdep_repo_root, verbose=False, sync_timestamps=False):
        self.versions = {}
        self.locations = {}
        self.rdep_target = None
        self.use_local = False
        self.have_updated_packages = False

        self.rdep = Rdep(rdep_repo_root)
        self.rdep.verbose = verbose
        self.rdep.fast_check = True

        self._exported_paths = {}
        self._env_exported_paths = {}
        self._synched_package_dirs = []
        self._include_ignored_dirs = []

        if sync_timestamps:
            self.rdep.sync_timestamps()

        self.rdep.load_timestamps_for_fast_check()

    def sync(self,
            package,
            path_variable=None,
            env_path_variable=None,
            optional=False,
            do_not_include=False):
        target, pack = posixpath.split(package)

        self.rdep.targets = [target] if target else [self.rdep_target]

        version = self.versions.get(pack)
        full_package_name = pack + "-" + version if version else pack
        path = self.locations.get(pack)

        if not path:
            sync_func = self.rdep.locate_package if self.use_local else self.rdep.sync_package

            if not sync_func(full_package_name):
                if not optional:
                    print("error: Cannot sync package {}".format(package), file=sys.stderr)
                    exit(1)

                return False

            path = self.rdep.locate_package(full_package_name)

        path = path.replace("\\", "/")

        self._synched_package_dirs.append(path)
        if path_variable:
            self._exported_paths[path_variable] = path
        if env_path_variable:
            self._env_exported_paths[env_path_variable] = path
        if do_not_include:
            self._include_ignored_dirs.append(path)

        self.have_updated_packages = self.rdep.is_package_updated(target, full_package_name)

        return True

    def generate_cmake_include(self, cmake_include_file, sync_script_file):
        if not self._need_to_write_depencdencies_file(cmake_include_file, sync_script_file):
            return

        with open(cmake_include_file, "w") as f:
            f.write("# Package versions.\n")
            for k, v in self.versions.items():
                f.write("set({0}_version \"{1}\")\n".format(k, v))

            f.write("\n# Exported package directories.\n")
            for k, v in self._exported_paths.items():
                f.write("set({0} \"{1}\")\n".format(k, v))

            f.write("\n# Package directories exported to environment.\n")
            for k, v in self._env_exported_paths.items():
                f.write("set(ENV{{{0}}} \"{1}\")\n".format(k, v))

            f.write("\n# List of synchronized package directories.\n")
            f.write("set(synched_package_dirs\n")
            for path in self._synched_package_dirs:
                f.write("    \"{}\"\n".format(path))
            f.write(")\n")

            f.write("\n# Includes.\n")
            for path in self._synched_package_dirs:
                if path in self._include_ignored_dirs:
                    continue

                cmake_files = glob.glob(os.path.join(path, "*.cmake"))
                if cmake_files and len(cmake_files) == 1:
                    f.write("include(\"{}\")\n".format(cmake_files[0].replace("\\", "/")))

    def _need_to_write_depencdencies_file(self,
        cmake_include_file: str, sync_script_file: str) -> bool:

        if self.have_updated_packages:
            return True

        if not Path(cmake_include_file).exists():
            return True

        # Check whether sync_dependencies.py was updated.
        return Path(sync_script_file).stat().st_mtime > Path(cmake_include_file).stat().st_mtime
