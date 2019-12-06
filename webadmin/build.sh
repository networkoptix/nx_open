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
echo "Update sources" >&2
for entry in $(ls -A "$SOURCE_DIR")
do
    [ -e "$entry" ] && rm -r "$entry"
    cp -pr "$SOURCE_DIR/$entry" "$entry"
done

echo "Clean old directories" >&2
[ -e static ] && rm -r static
[ -e server-external ] && rm -r server-external
[ -e external.dat ] && rm external.dat

# Install dependencies.
echo "Install node dependencies" >&2
npm install

# Build webadmin.
echo "Build webadmin" >&2
npm run build
mv dist static

# Make translations
echo "Create translations" >&2
pushd translation
    ./localize.sh
popd

# Save the repository info.
echo "Create version.txt" >&2

REP_ROOT_DIR="$SOURCE_DIR/.."
if [ -d "$REP_ROOT_DIR/.hg" ]; then
    hg log -r . --repository "$REP_ROOT_DIR" > static/version.txt
elif [ -d "$REP_ROOT_DIR/.git" ]; then
    git -C "$REP_ROOT_DIR" log -n 1 > static/version.txt
else
    echo "Neither git nor hg has been detected in $REP_ROOT_DIR" && exit 1
fi

cat static/version.txt >&2

#Pack
echo "Pack external.dat" >&2
zip -r external.dat ./static
mkdir -p ./server-external/bin
mv external.dat server-external/bin/external.dat

echo "Webadmin build done" >&2
