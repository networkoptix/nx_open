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

for entry in $(ls -A "$SOURCE_DIR")
do
    [ -e "$entry" ] && rm -r "$entry"
    cp -pr "$SOURCE_DIR/$entry" "$entry"
done

[ -e static ] && rm -r static
[ -e node_modules] && rm -r node_modules
[ -e server-external ] && rm -r server-external

# Save the repository info.

hg log -r . --repository "$SOURCE_DIR/.." > version.txt

# Install dependencies.
npm install

# Build webadmin.
npm run build
mv dist static

# Make translations
pushd translation
    ./localize.sh
popd

#Pack
tar -czvf external.dat ./static