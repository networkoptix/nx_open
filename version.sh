export OLD_VERSION=2.1.0-SNAPSHOT
export NEW_VERSION=2.2.0-SNAPSHOT

OLD_VERSION=${OLD_VERSION//./\\.}

echo $OLD_VERSION

#echo "for f in `find * -name pom.xml`; do sed -i"" -e 's/$OLD_VERSION/$NEW_VERSION/g' $f; done"

for f in `find * -name pom.xml`; do sed -i"" -e "s/$OLD_VERSION/$NEW_VERSION/g" $f; done

#~/buildenv/maven/bin/mvn deploy -N
