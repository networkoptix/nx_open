#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_utils/linux/build_distribution_utils.sh"

distr_loadConfig "build_distribution.conf"

TARGET="/opt/$CUSTOMIZATION/client/$VERSION"
USRTARGET="/usr"
BINTARGET="$TARGET/bin"
BGTARGET="$TARGET/share/pictures/sample-backgrounds"
HELPTARGET="$TARGET/help"
ICONTARGET="$USRTARGET/share/icons"
LIBTARGET="$TARGET/lib"
INITTARGET="/etc/init"
INITDTARGET="/etc/init.d"

WORK_DIR="client_build_distribution_tmp"
STAGE="$WORK_DIR/$ARTIFACT_NAME"
STAGETARGET="$STAGE/$TARGET"
BINSTAGE="$STAGE$BINTARGET"
BGSTAGE="$STAGE$BGTARGET"
HELPSTAGE="$STAGE$HELPTARGET"
ICONSTAGE="$STAGE$ICONTARGET"
LIBSTAGE="$STAGE$LIBTARGET"

CLIENT_BIN_PATH="$BUILD_DIR/bin"
CLIENT_IMAGEFORMATS_PATH="$CLIENT_BIN_PATH/imageformats"
CLIENT_MEDIASERVICE_PATH="$CLIENT_BIN_PATH/mediaservice"
CLIENT_AUDIO_PATH="$CLIENT_BIN_PATH/audio"
CLIENT_XCBGLINTEGRATIONS_PATH="$CLIENT_BIN_PATH/xcbglintegrations"
CLIENT_PLATFORMINPUTCONTEXTS_PATH="$CLIENT_BIN_PATH/platforminputcontexts"
CLIENT_QML_PATH="$CLIENT_BIN_PATH/qml"
CLIENT_VOX_PATH="$CLIENT_BIN_PATH/vox"
CLIENT_PLATFORMS_PATH="$CLIENT_BIN_PATH/platforms"
CLIENT_BG_PATH="$BUILD_DIR/backgrounds"
ICONS_PATH="$CUSTOMIZATION_DIR/icons/linux/hicolor"
CLIENT_LIB_PATH="$BUILD_DIR/lib"
BUILD_INFO_TXT="$BUILD_DIR/build_info.txt"
LOG_FILE="$LOGS_DIR/client_build_distribution.log"

#--------------------------------------------------------------------------------------------------

buildDistribution()
{
    local FILE

    echo "Creating directories"
    mkdir -p "$BINSTAGE/imageformats"
    mkdir -p "$BINSTAGE/mediaservice"
    mkdir -p "$BINSTAGE/audio"
    mkdir -p "$BINSTAGE/platforminputcontexts"
    mkdir -p "$HELPSTAGE"
    mkdir -p "$LIBSTAGE"
    mkdir -p "$BGSTAGE"
    mkdir -p "$ICONSTAGE"
    mkdir -p "$STAGE/etc/xdg/$FULL_CUSTOMIZATION"

    echo "Copying client.conf and applauncher.conf"
    cp debian/client.conf "$STAGE/etc/xdg/$FULL_CUSTOMIZATION/$FULL_PRODUCT_NAME"
    cp debian/applauncher.conf "$STAGE/etc/xdg/$FULL_CUSTOMIZATION/$FULL_APPLAUNCHER_NAME"

    echo "Copying build_info.txt"
    cp -r "$BUILD_INFO_TXT" "$BINSTAGE/../"

    echo "Copying qt.conf"
    cp -r qt.conf "$BINSTAGE/"

    echo "Copying client binaries and old version libs"
    cp -r "$CLIENT_BIN_PATH/$CLIENT_BINARY_NAME" "$BINSTAGE/"
    cp -r "$CLIENT_BIN_PATH/$APPLAUNCHER_BINARY_NAME" "$BINSTAGE/"
    cp -r "bin/client" "$BINSTAGE/"
    cp -r "$CLIENT_BIN_PATH/$LANUCHER_VERSION_FILE" "$BINSTAGE/"
    cp -r "bin/applauncher" "$BINSTAGE/"

    echo "Copying icons"
    cp -P -Rf usr "$STAGE/"
    mv -f "$STAGE/usr/share/applications/icon.desktop" \
        "$STAGE/usr/share/applications/$INSTALLER_NAME.desktop"
    mv -f "$STAGE/usr/share/applications/protocol.desktop" \
        "$STAGE/usr/share/applications/$URI_PROTOCOL.desktop"
    cp -P -Rf "$ICONS_PATH" "$ICONSTAGE/"
    for FILE in $(find "$ICONSTAGE" -name "*.png")
    do
        mv "$FILE" "$(dirname "$FILE")/$(basename "$FILE" .png)-$CUSTOMIZATION_NAME.png"
    done

    echo "Copying help"
    cp -r "$CLIENT_HELP_PATH"/* "$HELPSTAGE/"

    echo "Copying fonts"
    cp -r "$CLIENT_BIN_PATH/fonts" "$BINSTAGE/"

    echo "Copying backgrounds"
    cp -r "$CLIENT_BG_PATH"/* "$BGSTAGE"/

    local LIB
    local LIB_BASENAME
    for LIB in "$CLIENT_LIB_PATH"/*.so*
    do
        LIB_BASENAME=$(basename "$LIB")
        if [[ "$LIB_BASENAME" != libQt5* \
            && "$LIB_BASENAME" != libEnginio.so* \
            && "$LIB_BASENAME" != libqgsttools_p.* \
            && "$LIB_BASENAME" != libmediaserver* ]]
        then
            echo "Copying $LIB_BASENAME"
            cp -P "$LIB" "$LIBSTAGE/"
        fi
    done

    echo "Copying (Qt plugin) platforminputcontexts"
    cp -r "$CLIENT_PLATFORMINPUTCONTEXTS_PATH"/*.* "$BINSTAGE/platforminputcontexts/"
    echo "Copying (Qt plugin) imageformats"
    cp -r "$CLIENT_IMAGEFORMATS_PATH"/*.* "$BINSTAGE/imageformats/"
    echo "Copying (Qt plugin) mediaservice"
    cp -r "$CLIENT_MEDIASERVICE_PATH"/*.* "$BINSTAGE/mediaservice/"
    echo "Copying (Qt plugin) xcbglintegrations"
    cp -r "$CLIENT_XCBGLINTEGRATIONS_PATH" "$BINSTAGE/"
    echo "Copying (Qt plugin) qml"
    cp -r "$CLIENT_QML_PATH" "$BINSTAGE/"
    if [ "$ARCH" != "arm" ]
    then
        echo "Copying (Qt plugin) audio"
        cp -r "$CLIENT_AUDIO_PATH"/*.* "$BINSTAGE/audio/"
    fi
    echo "Copying (Qt plugin) platforms"
    cp -r "$CLIENT_PLATFORMS_PATH" "$BINSTAGE/"

    echo "Copying Festival VOX files"
    cp -r "$CLIENT_VOX_PATH" "$BINSTAGE/"

    echo "Deleting .debug libs (if any)"
    rm -f "$LIBSTAGE"/*.debug

    local QT_LIBS=(
        Core
        Gui
        Widgets
        WebKit
        WebChannel
        WebKitWidgets
        OpenGL
        Multimedia
        MultimediaQuick_p
        Qml
        Quick
        QuickWidgets
        LabsTemplates
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
        QT_LIBS+=( Sensors )
    fi
    local QT_LIB
    for QT_LIB in "${QT_LIBS[@]}"
    do
        FILE="libQt5$QT_LIB.so"
        echo "Copying (Qt) $FILE"
        cp -P "$QT_DIR/lib/$FILE"* "$LIBSTAGE/"
    done

    distr_cpSysLib "$LIBSTAGE" "${CPP_RUNTIME_LIBS[@]}"

    if [ "$ARCH" != "arm" ]
    then
        echo "Copying additional libs"
        distr_cpSysLib "$LIBSTAGE" \
            libXss.so.1 \
            libopenal.so.1
        distr_cpSysLib "$LIBSTAGE" libpng12.so.0 \
            || distr_cpSysLib "$LIBSTAGE" libpng.so
        cp -P "$QT_DIR/lib"/libicu*.so* "$LIBSTAGE"
    fi

    echo "Setting permissions"
    # TODO: Investigate whether settings permissions for the entire build dir contents is ok.
    find -type d -print0 |xargs -0 chmod 755
    find -type f -print0 |xargs -0 chmod 644
    chmod 755 "$BINSTAGE"/*

    echo "Preparing DEBIAN dir"
    mkdir -p "$STAGE/DEBIAN"
    chmod g-s,go+rx "$STAGE/DEBIAN"

    INSTALLED_SIZE=$(du -s "$STAGE" |awk '{print $1;}')

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

    echo "Creating .deb $ARTIFACT_NAME.deb"
    (cd "$WORK_DIR"
        fakeroot dpkg-deb -b "$ARTIFACT_NAME"
    )

    echo "Copying scalable/apps icons"
    mkdir -p "$STAGETARGET/share/icons"
    cp "$ICONSTAGE/hicolor/scalable/apps"/* "$STAGETARGET/share/icons"

    echo "Copying update.json"
    cp "bin/update.json" "$STAGETARGET"

    echo "Generating finalname-client.properties"
    echo "client.finalName=$ARTIFACT_NAME" >>finalname-client.properties

    echo "Creating .zip $UPDATE_NAME"
    (cd "$STAGETARGET"; zip -y -r "$UPDATE_NAME" ./*)

    if [ ! -z "$DISTRIBUTION_OUTPUT_DIR" ]
    then
        echo "Moving distribution .zip and .deb to $DISTRIBUTION_OUTPUT_DIR"
        mv "$STAGETARGET/$UPDATE_NAME" "$DISTRIBUTION_OUTPUT_DIR/"
        mv "$WORK_DIR/$ARTIFACT_NAME.deb" "$DISTRIBUTION_OUTPUT_DIR/"
    else
        echo "Moving distribution .zip to $CURRENT_BUILD_DIR"
        mv "$STAGETARGET/$UPDATE_NAME" "$CURRENT_BUILD_DIR/"
    fi
}

main()
{
    distr_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
