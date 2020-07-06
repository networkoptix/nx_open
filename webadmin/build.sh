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

SKINS="$SOURCE_DIR/../cloud_portal/skins"
FRONT_END_HM="$SOURCE_DIR/../cloud_portal/health_monitor"
# Check if health monitor exists.
if [[ -e "$FRONT_END_HM/webpack.health_monitor.js" ]]
then
    # Build Health Monitor
    echo "Building Health Monitor"
    HEALTH_MONITOR="health_monitor"
    FRONT_END="$HEALTH_MONITOR/health_monitor"

    [ -d $HEALTH_MONITOR ] && rm -rf $HEALTH_MONITOR
    mkdir -p $HEALTH_MONITOR

    echo "Copying skins"
    cp -r $SKINS $HEALTH_MONITOR

    echo "Copying health monitor sources to $HEALTH_MONITOR"
    cp -r $FRONT_END_HM $HEALTH_MONITOR

    echo "Building Health Monitor"
    pushd $FRONT_END

    echo "Node version"
    npm -v
    echo

    echo "Installing npm packages"
    npm install
    echo "npm Packages Installed"
    echo

    npm run setSkin blue

    echo "Current path"
    pwd

    echo "Building health monitor"
    npm run build
    echo "Health monitor built"
    echo

    echo "Renaming index.html to health.html."
    mv dist/index.html dist/health.html
    popd

    echo "COPY front end to dist"
    cp -r $FRONT_END/dist/{app.component.scss,fonts,icons,images,health.html,scripts,scripts,styles,language_compiled.json} static
    echo "Done with HM"
fi

# Make translations
echo "Create translations" >&2
pushd translation
    ./localize.sh
popd

# Save the repository info.
echo "Create version.txt" >&2

REP_ROOT_DIR="$SOURCE_DIR/.."
if [ -d "$REP_ROOT_DIR/.git" ]; then
    format="changeset: %H%nrefs: %D%nparents: %P%nauthor: %aN <%aE>%ndate: %ad%nsummary: %s"
    git -C "$REP_ROOT_DIR" show -s --format="$format" > static/version.txt
else
    echo "VCS has not been detected in $REP_ROOT_DIR" && exit 1
fi

cat static/version.txt >&2

#Pack
echo "Pack external.dat" >&2
zip -r external.dat ./static
mkdir -p ./server-external/bin
mv external.dat server-external/bin/external.dat

echo "Webadmin build done" >&2
