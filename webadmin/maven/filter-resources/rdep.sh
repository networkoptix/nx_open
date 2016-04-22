#!/bin/bash
export NX_REPOSITORY=$environment/packages
cp -f ${basedir}/external.dat ${environment.dir}/packages/any/server-external-${release.version}/bin
python ${root.dir}/build_utils/python/rdep.py -u -t any server-external-${release.version}