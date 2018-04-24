#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#If we are not using the repository we should update necessary files
if [[ "$PWD" != */nx_vms/cloud_portal* ]]
then
    echo "Updating Cloud Portal Content"
    if [ -e "cloud_portal" ]
    then
        pushd cloud_portal
            for entry in $(ls -A .)
            do
                [ -e "$entry" ] && rm -rf "$entry"
                cp -pr "$DIR/../$entry" "$entry"
                echo "Updated $entry"
            done
        popd
    else
        cp -pr $DIR/.. cloud_portal
    fi

    ls $PWD
    mkdir -p webadmin
    pushd webadmin
        [ -d 'app/web_common' ] && rm -rf app/web_common
        mkdir -p app/web_common && cp -pr $DIR/../../webadmin/app/web_common/* "$_"

        [ -d 'app/styles' ] && rm -rf app/styles
        mkdir -p app/styles && cp -pr $DIR/../../webadmin/app/styles/* "$_"

        [ -e 'package.json' ] && rm -rf 'package.json'
        cp $DIR/../../webadmin/package.json ./
    popd
    cd cloud_portal
else
    cd $DIR/..
fi

echo "pip install requirements"
[ -z "env" ] && virtualenv env
. ./env/bin/activate
pip install -r build_scripts/requirements.txt

pushd front_end
    echo "npm install cloud portal"
    npm install
popd
pushd ../webadmin
    echo "npm install webadmin"
    npm install
popd

cd build_scripts

TARGET_DIR="../cloud/static"

echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR
echo "Create $TARGET_DIR"
mkdir -p $TARGET_DIR


echo "Iterate all customizations"
for dir in ../customizations/*/
do
    dir=${dir%*/}
    CUSTOMIZATION=${dir/..\/customizations\//}
    ./build_customization.sh $CUSTOMIZATION $DIR
done

echo "Cloud portal build is finished"
# say "Cloud portal build is finished"
