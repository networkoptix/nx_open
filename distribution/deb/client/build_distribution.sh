#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="client_build_distribution_tmp"
LOG_FILE="$LOGS_DIR/client_build_distribution.log"

STAGE="$WORK_DIR/$DISTRIBUTION_NAME"
STAGE_MODULE="$STAGE/opt/$CUSTOMIZATION/client/$VERSION"
STAGE_BIN="$STAGE_MODULE/bin"
STAGE_LIB="$STAGE_MODULE/lib"
STAGE_ICONS="$STAGE/usr/share/icons"

#--------------------------------------------------------------------------------------------------

# [in] STAGE_BIN
copyBins()
{
    echo "Copying client binaries"
    cp -r "$BUILD_DIR/bin/$CLIENT_BINARY_NAME" "$STAGE_BIN/"
    cp -r "$BUILD_DIR/bin/$APPLAUNCHER_BINARY_NAME" "$STAGE_BIN/"
    cp -r "bin/client" "$STAGE_BIN/"
    cp -r "$BUILD_DIR/bin/$LAUNCHER_VERSION_FILE" "$STAGE_BIN/"
    cp -r "bin/applauncher" "$STAGE_BIN/"
}

# [in] STAGE
# [in] STAGE_ICONS
copyIcons()
{
    echo "Copying icons"
    mkdir -p "$STAGE_ICONS"
    cp -r usr "$STAGE/"
    mv -f "$STAGE/usr/share/applications/icon.desktop" \
        "$STAGE/usr/share/applications/$INSTALLER_NAME.desktop"
    mv -f "$STAGE/usr/share/applications/protocol.desktop" \
        "$STAGE/usr/share/applications/$URI_PROTOCOL.desktop"
    cp -r "$CUSTOMIZATION_DIR/icons/linux/hicolor" "$STAGE_ICONS/"
    for FILE in $(find "$STAGE_ICONS" -name "*.png")
    do
        mv "$FILE" "$(dirname "$FILE")/$(basename "$FILE" .png)-$CUSTOMIZATION_NAME.png"
    done
}

# [in] STAGE_MODULE
copyHelp()
{
    echo "Copying help"
    local -r STAGE_HELP="$STAGE_MODULE/help"
    mkdir -p "$STAGE_HELP"
    cp -r "$CLIENT_HELP_PATH"/* "$STAGE_HELP/"
}

# [in] STAGE_BIN
copyFonts()
{
    echo "Copying fonts"
    cp -r "$BUILD_DIR/bin/fonts" "$STAGE_BIN/"

}

# [in] STAGE_MODULE
copyBackgrounds()
{
    echo "Copying backgrounds"
    local -r STAGE_BACKGROUNDS="$STAGE_MODULE/share/pictures/sample-backgrounds"
    mkdir -p "$STAGE_BACKGROUNDS"
    cp -r "$BUILD_DIR/backgrounds"/* "$STAGE_BACKGROUNDS/"
}

# [in] STAGE_LIB
copyLibs()
{
    echo ""
    echo "Copying libs"

    mkdir -p "$STAGE_LIB"
    local LIB
    local LIB_BASENAME
    for LIB in "$BUILD_DIR/lib"/*.so*
    do
        LIB_BASENAME=$(basename "$LIB")
        if [[ "$LIB_BASENAME" != libQt5* \
            && "$LIB_BASENAME" != libEnginio.so* \
            && "$LIB_BASENAME" != libqgsttools_p.* \
            && "$LIB_BASENAME" != libmediaserver* ]]
        then
            echo "  Copying $LIB_BASENAME"
            cp -P "$LIB" "$STAGE_LIB/"
        fi
    done

    # TODO: #mshevchenko: Why .debug files are not deleted for mediaserver .deb?
    echo "  Deleting .debug files (if any)"
    rm -f "$STAGE_LIB"/*.debug

    echo "  Copying system libs: ${CPP_RUNTIME_LIBS[@]}"
    distrib_copySystemLibs "$STAGE_LIB" "${CPP_RUNTIME_LIBS[@]}"

    if [ "$ARCH" != "arm" ]
    then
        echo "  Copying additional system libs"
        distrib_copySystemLibs "$STAGE_LIB" libXss.so.1 libopenal.so.1
        distrib_copySystemLibs "$STAGE_LIB" libpng12.so.0 \
            || distrib_copySystemLibs "$STAGE_LIB" libpng.so
        distrib_copySystemLibs "$STAGE_LIB" "${ICU_RUNTIME_LIBS[@]}"
    fi
}

copyQtPlugins()
{
    echo ""
    echo "Copying Qt plugins"

    local QT_PLUGINS=(
        platforminputcontexts
        imageformats
        mediaservice
        xcbglintegrations
        platforms
        qml
    )
    if [ "$ARCH" != "arm" ]
    then
        QT_PLUGINS+=(
            audio
        )
    fi

    local QT_PLUGIN
    for QT_PLUGIN in "${QT_PLUGINS[@]}"
    do
        echo "  Copying (Qt plugin) $QT_PLUGIN"
        cp -r "$BUILD_DIR/bin/$QT_PLUGIN" "$STAGE_BIN/"
    done
}

# [in] STAGE_BIN
copyFestivalVox()
{
    echo "Copying Festival Vox files"
    cp -r "$BUILD_DIR/bin/vox" "$STAGE_BIN/"
}

# [in] STAGE_LIB
copyQtLibs()
{
    echo ""
    echo "Copying Qt libs"

    local QT_LIBS=(
        Core
        Gui
        Widgets
        WebKit
        WebChannel
        WebKitWidgets
        OpenGL
        Multimedia
        MultimediaQuick
        Qml
        Quick
        QuickWidgets
        QuickTemplates2
        QuickControls2
        X11Extras
        XcbQpa
        DBus
        Xml
        XmlPatterns
        Concurrent
        Network
        Sql
        PrintSupport
    )
    if [ "$ARCH" == "arm" ]
    then
        QT_LIBS+=(
            Sensors
        )
    fi

    local QT_LIB
    local FILE
    for QT_LIB in "${QT_LIBS[@]}"
    do
        FILE="libQt5$QT_LIB.so"
        echo "  Copying (Qt) $FILE"
        cp -P "$QT_DIR/lib/$FILE"* "$STAGE_LIB/"
    done
}

# [in] STAGE
createDebianDir()
{
    echo "Preparing DEBIAN dir"
    mkdir -p "$STAGE/DEBIAN"
    chmod g-s,go+rx "$STAGE/DEBIAN"

    local -r INSTALLED_SIZE=$(du -s "$STAGE" |awk '{print $1;}')

    echo "Generating DEBIAN/control"
    cat "debian/control.template" \
        |sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" \
        |sed "s/VERSION/$RELEASE_VERSION/g" \
        |sed "s/ARCHITECTURE/$ARCHITECTURE/g" \
        >"$STAGE/DEBIAN/control"

    echo "Copying DEBIAN dir"
    for FILE in $(ls debian)
    do
        if [ "$FILE" != "control.template" ]
        then
            install -m 755 "debian/$FILE" "$STAGE/DEBIAN/"
        fi
    done

    echo "Generating DEBIAN/md5sums"
    (cd "$STAGE"
        find * -type f -not -regex '^DEBIAN/.*' -print0 |xargs -0 md5sum >"DEBIAN/md5sums"
        chmod 644 "DEBIAN/md5sums"
    )
}

# [in] STAGE_MODULE
# [in] STAGE_ICONS
createUpdateZip()
{
    echo ""
    echo "Creating \"update\" .zip"

    echo "  Copying icon(s) for \"update\" .zip"
    local -r STAGE_UPDATE_ICONS="$STAGE_MODULE/share/icons"
    mkdir -p "$STAGE_UPDATE_ICONS"
    cp "$STAGE_ICONS/hicolor/scalable/apps"/* "$STAGE_UPDATE_ICONS/"

    echo "  Copying update.json for \"update\" .zip"
    cp "update/update.json" "$STAGE_MODULE/"

    distrib_createArchive "$DISTRIBUTION_OUTPUT_DIR/$UPDATE_ZIP" "$STAGE_MODULE" \
        zip -r `# Preserve symlinks #`-y
}

buildDistribution()
{
    local FILE

    echo "Copying client.conf and applauncher.conf"
    mkdir -p "$STAGE/etc/xdg/$FULL_CUSTOMIZATION"
    cp debian/client.conf "$STAGE/etc/xdg/$FULL_CUSTOMIZATION/$FULL_PRODUCT_NAME"
    cp debian/applauncher.conf "$STAGE/etc/xdg/$FULL_CUSTOMIZATION/$FULL_APPLAUNCHER_NAME"

    echo "Copying build_info.txt"
    mkdir -p "$STAGE_MODULE/"
    cp "$BUILD_DIR/build_info.txt" "$STAGE_MODULE/"

    echo "Copying qt.conf"
    mkdir -p "$STAGE_BIN"
    cp qt.conf "$STAGE_BIN/"

    copyBins
    copyIcons
    copyHelp
    copyFonts
    copyBackgrounds
    copyLibs
    copyQtPlugins
    copyFestivalVox

    copyQtLibs

    echo "Setting permissions"
    find "$STAGE" -type d -print0 |xargs -0 chmod 755
    find "$STAGE" -type f -print0 |xargs -0 chmod 644
    chmod 755 "$STAGE_BIN"/* #< Restore executable permission for the files in "bin".

    createDebianDir

    local -r DEB="$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_NAME.deb"

    echo "Creating $DEB"
    fakeroot dpkg-deb -b "$STAGE" "$DEB"

    createUpdateZip

    # TODO: This file seems to go nowhere. Is it needed? If yes, add a comment.
    echo "Generating finalname-client.properties"
    echo "client.finalName=$DISTRIBUTION_NAME" >>finalname-client.properties
}

#--------------------------------------------------------------------------------------------------

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
