#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ "$DIR" = "$PWD" ]
then
    echo "Error: $0 must not be executed from the sources directory." >&2
    exit 1
fi


echo "Cleaning current directory"
rm -rf *


echo "Copying source files"
cp -pr $DIR/.. cloud_portal
mkdir -p webadmin/app/web_common && cp -pr $DIR/../../webadmin/app/web_common/* "$_"
mkdir -p webadmin/app/styles && cp -pr $DIR/../../webadmin/app/styles/* "$_"
cp $DIR/../../webadmin/package.json webadmin


# Save the repository info.
echo "Create version.txt"
hg log -r . --repository "$DIR/../.." > cloud_portal/front_end/version.txt
cat cloud_portal/front_end/version.txt

echo "Move into cloud portal"
cd cloud_portal


echo "pip install requirements"
virtualenv env
. ./env/bin/activate
pip install -r ./build_scripts/requirements.txt


echo "npm install"
pushd front_end
npm install
popd

pushd ../webadmin
npm install
popd

TARGET_DIR="cloud/static"

echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR
echo "Create $TARGET_DIR"
mkdir $TARGET_DIR


echo "Iterate all customizations"
for CUSTOMIZATION in $(ls 'customizations')
do
    build_scripts/build_customization.sh $CUSTOMIZATION
done

echo "Done!"

# say "Cloud portal build is finished"
