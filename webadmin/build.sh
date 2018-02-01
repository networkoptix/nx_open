#!/bin/bash

set -e

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check prerequisites.

if [ "$SOURCE_DIR" = "$PWD" ]
then
    echo "Error: $0 must not be executed from the sources directory."
    exit 1
fi

compass --version &> /dev/null || (echo "Error: Ruby Compass is not installed." >&2 && exit 1)

# Update sources.

for entry in $(ls -A "$SOURCE_DIR")
do
    [ -e "$entry" ] && rm -r "$entry"
    cp -pr "$SOURCE_DIR/$entry" "$entry"
done

[ -e static ] && rm -r static
[ -e server-external ] && rm -r server-external

# Save the repository info.

hg log -r . --repository "$SOURCE_DIR/.." > version.txt

# Install dependencies.

npm install
node_modules/bower/bin/bower install

# Build webadmin.

node_modules/grunt-cli/bin/grunt publish
