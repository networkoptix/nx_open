#!/bin/bash
mkdir -p ${environment.dir}/packages/any/server-external-${branch}-${release.version}/bin
cp -f ${basedir}/external.dat ${environment.dir}/packages/any/server-external-${branch}-${release.version}/bin
if [[ $? -ne 0 ]]; then exit $?; fi
python ${root.dir}/build_utils/python/rdep.py -u -f -t any server-external-${branch}-${release.version} --root ${environment.dir}/packages
if [[ $? -ne 0 ]]; then exit $?; fi
if [[ $branch == prod_${release.version} ]]; then 
    mkdir -p ${environment.dir}/packages/any/server-external-${release.version}/bin
    cp -f ${basedir}/external.dat ${environment.dir}/packages/any/server-external-${release.version}/bin
    python ${root.dir}/build_utils/python/rdep.py -u -f -t any server-external-${release.version} --root ${environment.dir}/packages
    if [[ $? -ne 0 ]]; then exit $?; fi
fi
