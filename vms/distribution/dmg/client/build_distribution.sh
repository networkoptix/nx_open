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
PACKAGE_ICON_PATH="$SRC/.VolumeIcon.icns"
DMG_SETTINGS="settings.py"

BACKGROUND_PATH="$CUSTOMIZATION_DIR/desktop/macos/installer_background.png"

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
    local -r VERSION_DIR="Versions/A"
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
    cp "$BUILD_DIR/bin/client_core_external.dat" "$APP_RESOURCES_DIR"
    cp "$BUILD_DIR/bin/bytedance_iconpark.dat" "$APP_RESOURCES_DIR"
}

copyTranslations()
{
    echo "Copying translations"

    local -r STAGE_TRANSLATIONS="$APP_RESOURCES_DIR/translations"
    mkdir -p "$STAGE_TRANSLATIONS"

    local -r TRANSLATIONS=(
        nx_vms_common.dat
        nx_vms_license.dat
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

copyHelpFiles()
{
    echo "Copying Quick Start Guide"
    cp "${QUICK_START_GUIDE_FILE}" "$APP_RESOURCES_DIR"

    if [[ "${CUSTOMIZATION_MOBILE_CLIENT_ENABLED}" == "true" ]]
    then
        echo "Copying Mobile Help"
        cp "${MOBILE_HELP_FILE}" "$APP_RESOURCES_DIR"
    fi
}

copyMetadata()
{
    echo "Copying metadata"

    local -r metadataDir="${APP_DIR}/SharedSupport/metadata"

    mkdir -p "${metadataDir}"

    cp "${DISTRIBUTION_OUTPUT_DIR}/build_info.txt" "${metadataDir}"
    cp "${DISTRIBUTION_OUTPUT_DIR}/build_info.json" "${metadataDir}"
    cp "${DISTRIBUTION_OUTPUT_DIR}/conan_refs.txt" "${metadataDir}"
    cp "${DISTRIBUTION_OUTPUT_DIR}/conan.lock" "${metadataDir}"
}


buildDistribution()
{
    mv "$SRC/client.app" "$APP_DIR"
    mkdir -p "$APP_RESOURCES_DIR"
    cp "$APP_ICON" "$APP_RESOURCES_DIR/appIcon.icns"
    cp "$INSTALLER_ICON" "$SRC/.VolumeIcon.icns"
    cp -r "$ROBOTO_FONTS_DIRECTORY/bin/fonts" "$APP_RESOURCES_DIR/"

    copyMacOsSpecificApplauncherStuff

    PYTHONPATH="${VMS_DISTRIBUTION_COMMON_DIR}/scripts" "${PYTHON}" macdeployqt.py \
        "$CURRENT_BUILD_DIR" "$APP_DIR" "$BUILD_DIR/bin" "$BUILD_DIR/lib" "$CLIENT_HELP_PATH" \
        "$QT_DIR" "$QT_VERSION"

    copyWebEngineFiles
    copyResources
    copyTranslations
    copyHelpFiles
    copyMetadata

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

    if [ ${NOTARIZATION} = true ] && [ ${CODE_SIGNING} = true ]
    then
        # notarize.py can read notarization user and password from the current environment.
        # Check the variables to provide clearer error messages.
        if [ -z "${NOTARIZATION_USER}" ]
        then
            echo "Notarization is required, but the NOTARIZATION_USER variable is not set" >&2
            exit 1
        fi
        if [ -z "${NOTARIZATION_PASSWORD}" ]
        then
            echo "Notarization is required, but the NOTARIZATION_PASSWORD variable is not set" >&2
            exit 1
        fi

        "$PYTHON" notarize.py --team-id "${APPLE_TEAM_ID}" "${DISTRIBUTION_DMG}"
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
