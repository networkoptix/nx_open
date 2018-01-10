#!/bin/bash
set -x
set -e

BINARIES=@libdir@/bin/@build.configuration@
LIBRARIES=@libdir@/lib/@build.configuration@
SRC=./dmg-folder
TMP=tmp
VOLUME_NAME="@display.product.name@ @release.version@"
DMG_FILE="@artifact.name.client@.dmg"

# Take into consideration that we have protocol handler app with name
# @protocol_handler_app_name@ = @display.product.name@ Client.app.
# Please do not add "Client" word to APP_DIR because of that.
APP_DIR="$SRC/@display.product.name@.app"
HELP=@ClientHelpSourceDir@
RELEASE_VERSION=@release.version@
PROTOCOL_HANDLER_APP_NAME="@protocol_handler_app_name@"

QT_DIR="@qt.dir@"
QT_VERSION="@qt.version@"

ln -s /Applications $SRC/Applications

mv $SRC/client.app "$APP_DIR"
mv "$APP_DIR"/Contents/MacOS/protocol_handler.app "$APP_DIR"/Contents/MacOS/"$PROTOCOL_HANDLER_APP_NAME"
mkdir -p "$APP_DIR/Contents/Resources"
cp logo.icns "$APP_DIR/Contents/Resources/appIcon.icns"
cp logo.icns $SRC/.VolumeIcon.icns
cp -R "@packages.dir@/any/roboto-fonts/bin/fonts" "$APP_DIR/Contents/MacOS/"

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

patch_dsstore "$SRC/DS_Store" "$SRC/.DS_Store" $RELEASE_VERSION
rm "$SRC/DS_Store"

python macdeployqt.py "$APP_DIR" "$BINARIES" "$LIBRARIES" "$HELP" "$QT_DIR" "$QT_VERSION"
security unlock-keychain -p qweasd123 $HOME/Library/Keychains/login.keychain \
    || security unlock-keychain -p 123 $HOME/Library/Keychains/login.keychain

if [ '@mac.skip.sign@' == 'false'  ]
then
    codesign -f -v --deep -s "@mac.sign.identity@" "$APP_DIR"
fi

SetFile -c icnC $SRC/.VolumeIcon.icns
hdiutil create -srcfolder $SRC -volname "$VOLUME_NAME" -format UDRW -ov "raw-$DMG_FILE"

[ "$1" == "rwonly" ] && exit 0

mv update.json $SRC
cd dmg-folder
zip -y -r ../@artifact.name.client_update@.zip ./*.app *.json
cd ..

rm -rf $TMP
mkdir -p $TMP

hdiutil attach "raw-$DMG_FILE" -mountpoint "$TMP"
SetFile -a C "$TMP"
hdiutil detach "$TMP"
rm -rf "$TMP"
rm -f "$DMG_FILE"
hdiutil convert "raw-$DMG_FILE" -format UDZO -o "$DMG_FILE"
rm -f "raw-$DMG_FILE"
