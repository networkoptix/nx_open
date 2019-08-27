#!/bin/bash

export PATH="$RDEP_PACKAGES_DIR/${TARET:-linux-x64}/nx_box_tool-dev/python/bin:$PATH"

type -p pyinstaller

#pyinstaller -F --hidden-import json -p lib bin/nx_box_tool
