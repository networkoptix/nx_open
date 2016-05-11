#!/bin/bash
export NX_REPOSITORY=$environment/packages
mkdir -p ${environment.dir}/packages/any/server-external-${release.version}-${branch}/bin
cp -f ${basedir}/external.dat ${environment.dir}/packages/any/server-external-${release.version}-${branch}/bin
if [[ $? -ne 0 ]]; then exit $?; fi
python ${root.dir}/build_utils/python/rdep.py -u -t any server-external-${release.version}-${branch}
if [[ $? -ne 0 ]]; then exit $?; fi