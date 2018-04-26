#!/bin/bash
set -e

#DIR is the location of the cloud_portal build script in the repository
#Can be called like this from with cloud_portal/build_scripts "./build.sh"
# or from cloud_portal "./build_scripts/build.sh"
#or like this from outside the repository "../nx_vms/cloud_portal/build_scripts/build.sh"

NX_VMS="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/../.."

#If we are not using the repository we should update necessary files
if [[ "$PWD" != *cloud_portal* ]]
then
    echo "Updating Cloud Portal Content"
    if [ -e "cloud_portal" ]
    then
        pushd cloud_portal
            for entry in $(ls -A .)
            do
                if [ "$entry" = "front_end" ]
                then
                    pushd $entry
                    for element in $(ls -A .)
                    do
                        [ "$element" = "node_modules" ] && continue
                        [ -e "$element" ] && rm -rf "$element"
                        cp -pr "$NX_VMS/cloud_portal/$entry/$element" "$element"
                    done
                    popd
                else
                    [[ "$entry" = "env" ]] && continue
                    [ -e "$entry" ] && rm -rf "$entry"
                    cp -pr "$NX_VMS/cloud_portal/$entry" "$entry"
                fi
            done
        popd
    else
        cp -pr $NX_VMS/cloud_portal cloud_portal
    fi

    mkdir -p webadmin
    pushd webadmin
        [ -d 'app/web_common' ] && rm -rf app/web_common
        mkdir -p app/web_common && cp -pr $NX_VMS/webadmin/app/web_common/* "$_"

        [ -d 'app/styles' ] && rm -rf app/styles
        mkdir -p app/styles && cp -pr $NX_VMS/webadmin/app/styles/* "$_"

        [ -e 'package.json' ] && rm -rf 'package.json'
        cp $NX_VMS/webadmin/package.json ./
    popd
    cd cloud_portal
else
    cd $NX_VMS/cloud_portal
fi

echo "pip install requirements"
[ ! -d "env" ] && virtualenv env
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


echo "Iterate all skins"
for dir in ../skins/*/
do
    dir=${dir%*/}
    SKIN=${dir/..\/skins\//}
    ./build_skin.sh $SKIN $NX_VMS
done

cp ../cloud/cloud/cloud_portal.yaml $TARGET_DIR/_source


echo "Cloud portal build is finished"
# say "Cloud portal build is finished"
