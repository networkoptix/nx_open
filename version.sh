export OLD_VERSION=2.3.1-SNAPSHOT
export NEW_VERSION=2.3.2-SNAPSHOT

OLD_VERSION=${OLD_VERSION//./\\.}

echo $OLD_VERSION

#echo "for f in `find * -name pom.xml`; do sed -i"" -e 's/$OLD_VERSION/$NEW_VERSION/g' $f; done"

for f in `find * -name pom.xml`; do sed -i"" -e "s/$OLD_VERSION/$NEW_VERSION/g" $f; done

mvn deploy -N
