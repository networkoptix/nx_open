#!/bin/bash

set -e

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check prerequisites.

if [ "$SOURCE_DIR" = "$PWD" ]
then
    echo "Error: $0 must not be executed from the sources directory." >&2
    exit 1
fi

# Update sources.
echo "Update sources"
for entry in $(ls -A "$SOURCE_DIR")
do
    [ -e "$entry" ] && rm -r "$entry"
    cp -pr "$SOURCE_DIR/$entry" "$entry"
done

echo "Clean old directories"
[ -e static ] && rm -r static
[ -e dist ] && rm -r dist
[ -e server-external ] && rm -r server-external
[ -e external.dat ] && rm external.dat

# Save the repository info.
echo "Create version.txt"
hg log -r . --repository "$SOURCE_DIR/.." > version.txt

# Install dependencies.
echo "Install node dependencies"
npm install

# Build webadmin.
echo "Build webadmin"
npm run build
mv dist static

# Make translations
echo "Create translations"
pushd translation
    ./localize.sh
popd

#Pack
echo "Pack external.dat"
tar -czvf external.dat ./static