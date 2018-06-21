#!/bin/bash

set -x
set -e

BINARIES=@libdir@/bin/@build.configuration@
LIBRARIES=@libdir@/lib/@build.configuration@
SRC=./dmg-folder
TMP=tmp
VOLUME_NAME="@display.product.name@ @release.version@"
DMG_FILE="@artifact.name.client@.dmg"
KEYCHAIN="@codeSigningKeychainName@"

APP_DIR="$SRC/@display.product.name@.app"
HELP=@ClientHelpSourceDir@
RELEASE_VERSION=@release.version@

QT_DIR="@qt.dir@"
QT_VERSION="@qt.version@"

ln -s /Applications $SRC/Applications

mv $SRC/client.app "$APP_DIR"
mkdir -p "$APP_DIR/Contents/Resources"
cp logo.icns "$APP_DIR/Contents/Resources/appIcon.icns"
cp logo.icns $SRC/.VolumeIcon.icns
cp -R "@packages.dir@/any/roboto-fonts/bin/fonts" "$APP_DIR/Contents/Resources/"

function hexify
{
    hexdump -ve '1/1 "%.2X"'
}

function dehexify
{
    xxd -r -p
}

function patch_dsstore
{
    local xfrom="$1"
    local xto="$2"
    local version=$3
    local proshita=2.3.2

    local from_ver_hex=$(echo -ne $proshita | hexify)
    local to_ver_hex=$(echo -ne $version | hexify)

    cat $xfrom | hexify | sed "s/$from_ver_hex/$to_ver_hex/g" | dehexify > $xto
}

# Mac OS specific applauncher stuff

LAUNCHER_DIR=LAUNCHER.app
LAUNCHER_RESOURCES_DIR="$LAUNCHER_DIR/Contents/Resources"
APP_RESOURCES_DIR="$APP_DIR/Contents/Resources"

rm -rf $LAUNCHER_DIR
osacompile -o $LAUNCHER_DIR global_launcher.applescript

mkdir -p "$APP_RESOURCES_DIR/Scripts"
cp "$LAUNCHER_RESOURCES_DIR/Scripts/main.scpt" "$APP_RESOURCES_DIR/Scripts"
cp "$LAUNCHER_RESOURCES_DIR/droplet.rsrc" "$APP_RESOURCES_DIR/launcher.rsrc"
cp "$LAUNCHER_DIR/Contents/MacOS/droplet" "$APP_DIR/Contents/MacOS/launcher"

#

function hard_detach_dmg
{
    lsof -t "$1" | xargs kill -9
}

patch_dsstore "$SRC/DS_Store" "$SRC/.DS_Store" $RELEASE_VERSION
rm "$SRC/DS_Store"

python macdeployqt.py "$APP_DIR" "$BINARIES" "$LIBRARIES" "$HELP" "$QT_DIR" "$QT_VERSION"

if [ '@mac.skip.sign@' == 'false'  ]
then
    if [ -z "$KEYCHAIN" ]
    then
        KEYCHAIN_ARGS=
        security unlock-keychain -p qweasd123 $HOME/Library/Keychains/login.keychain \
            || security unlock-keychain -p 123 $HOME/Library/Keychains/login.keychain
    else
        KEYCHAIN_ARGS="--keychain $KEYCHAIN"
    fi

    codesign -f -v --deep $KEYCHAIN_ARGS -s "@mac.sign.identity@" "$APP_DIR"
fi

SetFile -c icnC $SRC/.VolumeIcon.icns

hard_detach_dmg "raw-$DMG_FILE"

hdiutil create -srcfolder $SRC -volname "$VOLUME_NAME" -fs "HFS+" -format UDRW -ov "raw-$DMG_FILE"

[ "$1" == "rwonly" ] && exit 0

mv update.json $SRC
cd dmg-folder
zip -y -r ../@artifact.name.client_update@.zip ./*.app *.json
cd ..

rm -rf $TMP
mkdir -p $TMP

hdiutil attach "raw-$DMG_FILE" -mountpoint "$TMP"
SetFile -a C "$TMP"

# Try detach multiple times because something in MacOS can sometimes hold the directory.
DETACHED=0
for i in {1..20}
do
    hdiutil detach "$TMP" && DETACHED=1 && break
    echo "Cannot detach $TMP" >&2
    sleep 1
done
[ $DETACHED -ne 1 ] && exit 1

rm -rf "$TMP"
rm -f "$DMG_FILE"
hard_detach_dmg "raw-$DMG_FILE"
hdiutil convert "raw-$DMG_FILE" -format UDZO -o "$DMG_FILE"
rm -f "raw-$DMG_FILE"
