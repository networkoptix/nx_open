#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="build_distribution_tmp"
LOG_FILE="$LOGS_DIR/build_distribution.log"

# TODO: Create the "stage" in $WORK_DIR.
RAW_DMG="raw-$DISTRIBUTION_DMG"
SRC=./dmg-folder
VOLUME_NAME="$DISPLAY_PRODUCT_NAME $RELEASE_VERSION"

APP_DIR="$SRC/$DISPLAY_PRODUCT_NAME.app"

hexify()
{
    hexdump -ve '1/1 "%.2X"'
}

dehexify()
{
    xxd -r -p
}

patchDsStore()
{
    local -r xFrom="$1"
    local -r xTo="$2"
    local -r version="$3"
    local -r hardCodedVersion="2.3.2"

    local -r fromVerHex=$(echo -ne "$hardCodedVersion" |hexify)
    local -r toVerHex=$(echo -ne "$version" |hexify)

    cat "$xFrom" |hexify |sed "s/$fromVerHex/$toVerHex/g" |dehexify >"$xTo"
}

hardDetachDmg() # file.dmg
{
    if [ -f "$1" ]
    then
        lsof -t "$1" |xargs kill -9
    fi
}

setDmgIcon()
{
    local -r attachedDmg="$WORK_DIR/attached_dmg"
    rm -rf "$attachedDmg"
    mkdir -p "$attachedDmg"

    hdiutil attach "$RAW_DMG" -mountpoint "$attachedDmg"
    SetFile -a C "$attachedDmg"

    # Try detach multiple times because something in MacOS can sometimes hold the directory.
    local -i detached=0
    for i in {1..20}
    do
        hdiutil detach "$attachedDmg" && detached=1 && break
        echo "Cannot detach $attachedDmg" >&2
        sleep 1
    done
    [[ $detached -ne 1 ]] && exit 1

    rm -rf "$attachedDmg"
}

copyMacOsSpecificApplauncherStuff()
{
    LAUNCHER_DIR=LAUNCHER.app
    LAUNCHER_RESOURCES_DIR="$LAUNCHER_DIR/Contents/Resources"
    APP_RESOURCES_DIR="$APP_DIR/Contents/Resources"

    rm -rf $LAUNCHER_DIR
    osacompile -o $LAUNCHER_DIR global_launcher.applescript

    mkdir -p "$APP_RESOURCES_DIR/Scripts"
    cp "$LAUNCHER_RESOURCES_DIR/Scripts/main.scpt" "$APP_RESOURCES_DIR/Scripts"
    cp "$LAUNCHER_RESOURCES_DIR/droplet.rsrc" "$APP_RESOURCES_DIR/launcher.rsrc"
    cp "$LAUNCHER_DIR/Contents/MacOS/droplet" "$APP_DIR/Contents/MacOS/launcher"
}

buildDistribution()
{
    ln -s /Applications "$SRC/Applications"

    mv "$SRC/client.app" "$APP_DIR"
    mkdir -p "$APP_DIR/Contents/Resources"
    cp "$APP_ICON" "$APP_DIR/Contents/Resources/appIcon.icns"
    cp "$INSTALLER_ICON" "$SRC/.VolumeIcon.icns"
    cp -r "$PACKAGES_DIR/any/roboto-fonts/bin/fonts" "$APP_DIR/Contents/Resources/"

    copyMacOsSpecificApplauncherStuff

    patchDsStore "$SRC/DS_Store" "$SRC/.DS_Store" "$RELEASE_VERSION"
    rm "$SRC/DS_Store"

    python macdeployqt.py \
        "$CURRENT_BUILD_DIR" "$APP_DIR" "$BUILD_DIR/bin" "$BUILD_DIR/lib" "$CLIENT_HELP_PATH" "$QT_DIR" \
        "$QT_VERSION"

    if [[ $CODE_SIGNING = true ]]
    then
        local KEYCHAIN_ARGS=""

        if [ ! -z "$KEYCHAIN" ]
        then
            KEYCHAIN_ARGS="--keychain $KEYCHAIN"
        fi

        codesign -f -v --deep $KEYCHAIN_ARGS -s "$MAC_SIGN_IDENTITY" "$APP_DIR"
    fi

    SetFile -c icnC "$SRC/.VolumeIcon.icns"

    hardDetachDmg "$RAW_DMG"

    hdiutil create \
        -srcfolder "$SRC" -volname "$VOLUME_NAME" -fs "HFS+" -format UDRW \
        -ov "$RAW_DMG"

    mv update.json "$SRC/"

    (cd "$SRC"
        zip -y -r "../$UPDATE_ZIP" *.app update.json
    )

    setDmgIcon

    rm -f "$DISTRIBUTION_DMG"
    hardDetachDmg "$RAW_DMG"
    hdiutil convert "$RAW_DMG" -format UDZO -o "$DISTRIBUTION_DMG"
    rm -f "$RAW_DMG"
}

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    set -x #< Log each command.

    buildDistribution
}

main "$@"
