#!/bin/bash
export NX_REPOSITORY=$environment/packages
python ${root.dir}/build_utils/python/rdep.py -u -t any server-external-${release.version}