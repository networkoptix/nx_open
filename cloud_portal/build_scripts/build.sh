#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

TARGET_DIR="../cloud/static"

echo "Clear $TARGET_DIR"
rm -rf $TARGET_DIR
echo "Create $TARGET_DIR"
mkdir $TARGET_DIR

echo "Iterate all skins"
for dir in ../skins/*/
do
    dir=${dir%*/}
    SKIN=${dir/..\/skins\//}

    ./build_skin.sh $SKIN
done

echo "Done!"

# say "Cloud portal build is finished"
