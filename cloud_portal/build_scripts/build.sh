#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

echo "pip install requirements"
virtualenv ../env
. ../env/bin/activate
pip install -r requirements.txt


echo "npm install"
pushd ../front_end
npm install
popd

pushd ../../webadmin
npm install
popd

TARGET_DIR="../cloud/static"

echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR
echo "Create $TARGET_DIR"
mkdir $TARGET_DIR


echo "Iterate all customizations"
for dir in ../customizations/*/
do
    dir=${dir%*/}
    CUSTOMIZATION=${dir/..\/customizations\//}
    ./build_customization.sh $CUSTOMIZATION
done

echo "Done!"

# say "Cloud portal build is finished"
