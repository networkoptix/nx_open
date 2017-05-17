#!/bin/bash

export OLD_VERSION=3.0.0-SNAPSHOT
export NEW_VERSION=3.0.1-SNAPSHOT

OLD_VERSION=${OLD_VERSION//./\\.}

echo $OLD_VERSION

#echo "for f in `find * -name pom.xml`; do sed -i"" -e 's/$OLD_VERSION/$NEW_VERSION/g' $f; done"

for f in `find * -name pom.xml`; do sed -i"" -e "s/$OLD_VERSION/$NEW_VERSION/g" $f; done

#for f in $( find ./customization -iname DS_Store ); do perl -pi -e 's/$OLD_VERSION/$NEW_VERSION/g' $f; done
mvn deploy -N

