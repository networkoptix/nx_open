#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

cd $DIR/..

rm -rf repo
mkdir repo
for f in $(find . -type f -name \*.deb)
do
    name=$(basename $f)
    name=${name/-2.3/_2.3}
    name=${name/-x/_x}
    ln $f repo/$name
done

(cd repo; dpkg-scanpackages . > Packages)
