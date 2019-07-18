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
DMG_SETTINGS="settings.py"

APP_BUNDLE="$DISPLAY_PRODUCT_NAME.app"
APP_DIR="$SRC/$APP_BUNDLE"

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

        codesign -f -v --options runtime --deep $KEYCHAIN_ARGS -s "$MAC_SIGN_IDENTITY" "$APP_DIR"
    fi
 
    test -f $DISTRIBUTION_DMG && rm $DISTRIBUTION_DMG
    dmgbuild \
        -s "$DMG_SETTINGS" \
        -D dmg_app_path="$APP_DIR" \
        -D dmg_app_bundle="$APP_BUNDLE" \
        -D dmg_icon="$PACKAGE_ICON_PATH" \
        -D dmg_background="$BACKGROUND_PATH" \
        "$VOLUME_NAME" "$DISTRIBUTION_DMG"
    
    if [ $NOTARIZATION = true ] && [ $CODE_SIGNING = true ]
    then
        # Use environment variable NOTARIZATION_PASSWORD if specified.
        # If it is unset we use KEYCHAIN_NOTARIZATION_USER_PASSWORD from login keychain. 
        # It can be usefull for development purposes.
        KEYCHAIN_PASSWORD="@keychain:KEYCHAIN_NOTARIZATION_USER_PASSWORD"
        FINAL_PASSWORD="${NOTARIZATION_PASSWORD:-$KEYCHAIN_PASSWORD}"
        
        # Use environment variable NOTARIZATION_USER if specified.
        # If it is unset we use KEYCHAIN_NOTARIZATION_USER from environment.
        # It can be usefull for development purposes.
        FINAL_USER="${NOTARIZATION_USER:-$KEYCHAIN_NOTARIZATION_USER}"

        python notarize.py notarize \
            --user "$FINAL_USER" \
            --password "$FINAL_PASSWORD" \
            --team-id "$APPLE_TEAM_ID" \
            --file-name "$DISTRIBUTION_DMG" \
            --bundle-id "$BUNDLE_ID" 
    fi
    
    mv update.json "$SRC/"
    cp package.json "$SRC/"

    (cd "$SRC"
        zip -y -r "../$UPDATE_ZIP" *.app update.json package.json
    )
}

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    set -x #< Log each command.

    buildDistribution
}

main "$@"
