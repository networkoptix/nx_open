export OLD_VERSION=1.4.1
export NEW_VERSION=1.4.2

OLD_VERSION=${OLD_VERSION//./\\.}

echo $OLD_VERSION

#echo "for f in `find * -name pom.xml`; do sed -i"" -e 's/$OLD_VERSION/$NEW_VERSION/g' $f; done"

for f in `find * -name pom.xml`; do sed -i"" -e "s/$OLD_VERSION/$NEW_VERSION/g" $f; done
