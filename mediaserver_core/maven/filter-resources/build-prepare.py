#!/usr/bin/env python

import os, sys, posixpath
import shutil
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))
#shutil.copy(r'${root.dir}/nx_sdk/nx_sdk/vms-archive.zip', r'${project.build.directory}/resources/static/sdk.zip')
#shutil.copy(r'${root.dir}/nx_storage_sdk/nx_storage_sdk/vms-archive.zip', r'${project.build.directory}/resources/static/storage_sdk.zip')
shutil.copy(r'${project.build.Directory}/favicon.ico', r'${project.build.Directory}/resources/static/customization/favicon.ico')