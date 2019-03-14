#!/bin/bash
set -e

#DIR is the location of the cloud_portal build script in the repository
#Can be called like this from with cloud_portal/build_scripts "./build.sh"
# or from cloud_portal "./build_scripts/build.sh"
#or like this from outside the repository "../nx_vms/cloud_portal/build_scripts/build.sh"

VMS_REPOSITORY="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/../.."
[[ "$VMS_REPOSITORY" =~ (.*)\/cloud_portal.* ]]; REPO=${BASH_REMATCH[1]}

#If we are not using the repository we should update necessary files
if [[ ! $PWD =~ $REPO ]]
then
    echo "Updating Cloud Portal sources"
    if [ -e "cloud_portal" ]
    then
        pushd cloud_portal
            for entry in $(ls -A $VMS_REPOSITORY/cloud_portal/)
            do
                if [ "$entry" = "front_end" ]
                then
                    pushd $entry
                    for element in $(ls -A $VMS_REPOSITORY/cloud_portal/$entry/)
                    do
                        echo "copy $entry/$element"
                        [ -e "$element" ] && rm -rf "$element"
                        cp -pr "$VMS_REPOSITORY/cloud_portal/$entry/$element" "$element"
                    done
                    popd
                else
                    echo "copy $entry"
                    [ -e "$entry" ] && rm -rf "$entry"
                    cp -pr "$VMS_REPOSITORY/cloud_portal/$entry" "$entry"
                fi
            done
        popd
    else
        cp -pr $VMS_REPOSITORY/cloud_portal cloud_portal
    fi

    mkdir -p webadmin
    pushd webadmin
        [ -d 'app/styles' ] && rm -rf app/styles
        mkdir -p app/styles && cp -pr $VMS_REPOSITORY/webadmin/app/styles/* "$_"

        [ -e 'package.json' ] && rm -rf 'package.json'
        cp $VMS_REPOSITORY/webadmin/package.json ./
    popd
    cd cloud_portal
else
    echo "In repository skip copying sources"
    cd $VMS_REPOSITORY/cloud_portal
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
    ./build_skin.sh $SKIN $VMS_REPOSITORY
done

cp ../cloud/cloud/cloud_portal.yaml $TARGET_DIR/_source


echo "Cloud portal build is finished"
# say "Cloud portal build is finished"
