#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="build_distribution_tmp"
LOG_FILE="$LOGS_DIR/build_distribution.log"

# TODO: Create the "stage" in $WORK_DIR.
SRC=./dmg-folder
VOLUME_NAME="$DISPLAY_PRODUCT_NAME $RELEASE_VERSION"
BACKGROUND_PATH="$SRC/.background/dmgBackground.png"
PACKAGE_ICON_PATH="$SRC/.VolumeIcon.icns"
TMP_DMG_SETTINGS="settings.json"

APP_NAME="$DISPLAY_PRODUCT_NAME.app"
APP_DIR="$SRC/$APP_NAME"

buildDmg()
{
    test -f $DISTRIBUTION_DMG && rm $DISTRIBUTION_DMG

    echo "{
        \"title\": \"$VOLUME_NAME\",
        \"background\": \"$BACKGROUND_PATH\",
        \"format\": \"UDZO\",
        \"compression-level\": 9,
        \"icon\": \"$PACKAGE_ICON_PATH\",
        \"icon-size\": 64,
        \"window\": 
        { 
            \"position\": { \"x\": 200, \"y\": 200 },
            \"size\": { \"width\": 600, \"height\": 400 } 
        },
        \"contents\": 
        [
            { \"x\": 134, \"y\": 236, \"type\": \"file\", \"path\": \"$APP_DIR\" },
            { \"x\": 466, \"y\": 236, \"type\": \"link\", \"path\": \"/Applications\" }
        ]
    }
    " > "$TMP_DMG_SETTINGS"

    dmgbuild -s "$TMP_DMG_SETTINGS" "$VOLUME_NAME" "$DISTRIBUTION_DMG"
    rm -rf "$TMP_DMG_SETTINGS"
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
    mv "$SRC/client.app" "$APP_DIR"
    mkdir -p "$APP_DIR/Contents/Resources"
    cp "$APP_ICON" "$APP_DIR/Contents/Resources/appIcon.icns"
    cp "$INSTALLER_ICON" "$SRC/.VolumeIcon.icns"
    cp -r "$PACKAGES_DIR/any/roboto-fonts/bin/fonts" "$APP_DIR/Contents/Resources/"

    copyMacOsSpecificApplauncherStuff

    
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
 
    buildDmg

    mv update.json "$SRC/"
    cp package.json "$SRC/"

    (cd "$SRC"
        zip -y -r "../$UPDATE_ZIP" *.app update.json package.json
    )
}

main()
{
    # Supresses perl warnings.
    export LC_CTYPE=en_US.UTF-8
    export LC_ALL=en_US.UTF-8

    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    set -x #< Log each command.

    buildDistribution
}

main "$@"
