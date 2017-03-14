#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

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
