#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
APP_RESOURCES_DIR="$APP_DIR/Contents/Resources"

copyMacOsSpecificApplauncherStuff()
{
    LAUNCHER_DIR=LAUNCHER.app
    LAUNCHER_RESOURCES_DIR="$LAUNCHER_DIR/Contents/Resources"

    rm -rf $LAUNCHER_DIR
    osacompile -o $LAUNCHER_DIR global_launcher.applescript

    mkdir -p "$APP_RESOURCES_DIR/Scripts"
    cp "$LAUNCHER_RESOURCES_DIR/Scripts/main.scpt" "$APP_RESOURCES_DIR/Scripts"
    cp "$LAUNCHER_RESOURCES_DIR/droplet.rsrc" "$APP_RESOURCES_DIR/launcher.rsrc"
    cp "$LAUNCHER_DIR/Contents/MacOS/droplet" "$APP_DIR/Contents/MacOS/launcher"
}

copyWebEngineFiles()
{
    local -r FRAMEWORK="QtWebEngineCore.framework"
    local -r VERSION_DIR="Versions/5"
    local -r APP_FRAMEWORK_DIR="$APP_DIR/Contents/Frameworks/$FRAMEWORK"

    cp -r "$QT_DIR/lib/$FRAMEWORK/$VERSION_DIR/Helpers" "$APP_FRAMEWORK_DIR/$VERSION_DIR/"
    cp -r "$QT_DIR/lib/$FRAMEWORK//$VERSION_DIR/Resources" "$APP_FRAMEWORK_DIR/$VERSION_DIR/"
    ln -sf "Versions/Current/Helpers" "$APP_FRAMEWORK_DIR"
    ln -sf "Versions/Current/Resources" "$APP_FRAMEWORK_DIR"
}

copyResources()
{
    echo "Copying resources"
    cp "$BUILD_DIR/bin/client_external.dat" "$APP_RESOURCES_DIR"
    cp "$BUILD_DIR/bin/bytedance_iconpark.dat" "$APP_RESOURCES_DIR"
}

copyTranslations()
{
    echo "Copying translations"

    local -r STAGE_TRANSLATIONS="$APP_RESOURCES_DIR/translations"
    mkdir -p "$STAGE_TRANSLATIONS"

    local -r TRANSLATIONS=(
        nx_vms_common.dat
        nx_vms_client_core.dat
        nx_vms_client_desktop.dat
        nx_vms_rules.dat
    )

    for FILE in "${TRANSLATIONS[@]}"
    do
        echo "  Copying bin/translations/$FILE"
        cp "$BUILD_DIR/bin/translations/$FILE" "$STAGE_TRANSLATIONS"
    done
}

copyQuickStartGuide()
{
    echo "Copying QuickStartGuide"
    cp "${QUICK_START_GUIDE_FILE}" "$APP_RESOURCES_DIR"
}

buildDistribution()
{
    mv "$SRC/client.app" "$APP_DIR"
    mkdir -p "$APP_RESOURCES_DIR"
    cp "$APP_ICON" "$APP_RESOURCES_DIR/appIcon.icns"
    cp "$INSTALLER_ICON" "$SRC/.VolumeIcon.icns"
    cp -r "$ROBOTO_FONTS_DIRECTORY/bin/fonts" "$APP_RESOURCES_DIR/"

    copyMacOsSpecificApplauncherStuff

    "$PYTHON" macdeployqt.py \
        "$CURRENT_BUILD_DIR" "$APP_DIR" "$BUILD_DIR/bin" "$BUILD_DIR/lib" "$CLIENT_HELP_PATH" "$QT_DIR" \
        "$QT_VERSION"

    copyWebEngineFiles
    copyResources
    copyTranslations
    copyQuickStartGuide

    if [ $CODE_SIGNING = true ]
    then
        local KEYCHAIN_ARGS=""

        if [ ! -z "$KEYCHAIN" ]
        then
            KEYCHAIN_ARGS="--keychain $KEYCHAIN"
        fi

        "$PYTHON" "$OPEN_SOURCE_DIR/build_utils/macos/signtool.py" \
            sign \
            --identity "$MAC_SIGN_IDENTITY" \
            --entitlements "$CURRENT_SOURCE_DIR/entitlements.plist" \
            $KEYCHAIN_ARGS \
            "$APP_DIR"
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

        "$PYTHON" notarize.py notarize \
            --user "$FINAL_USER" \
            --password "$FINAL_PASSWORD" \
            --team-id "$APPLE_TEAM_ID" \
            --file-name "$DISTRIBUTION_DMG" \
            --bundle-id "$BUNDLE_ID"
    fi

    mv update.json "$SRC/"
    cp package.json "$SRC/"

    (cd "$SRC"
        zip -y -r -$ZIP_COMPRESSION_LEVEL "$UPDATE_ZIP" *.app update.json package.json
    )

    if [ $ARCHITECTURE = x64 ]
    then
        # Now we need to create one more update package with the same contents, but targeting
        # `macos` platform. This package will be used by old (<= 4.2) clients to update.

        sed -i "" "s/macos_x64/macos/" "$SRC/package.json"
        (cd "$SRC"
            zip -y -r -$ZIP_COMPRESSION_LEVEL "$COMPATIBILITY_UPDATE_ZIP" \
                *.app update.json package.json
        )
    fi
}

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    set -x #< Log each command.

    buildDistribution
}

main "$@"
