#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_utils/linux/build_distribution_utils.sh"

distr_loadConfig "build_distribution.conf"

TARGET="/opt/$CUSTOMIZATION/mediaserver"
BINTARGET="$TARGET/bin"
LIBTARGET="$TARGET/lib"
LIBPLUGINTARGET="$BINTARGET/plugins"
LIBPLUGINTARGET_OPTIONAL="$BINTARGET/plugins_optional"
SHARETARGET="$TARGET/share"
ETCTARGET="$TARGET/etc"
INITTARGET="/etc/init"
INITDTARGET="/etc/init.d"
SYSTEMDTARGET="/etc/systemd/system"

WORK_DIR="stagebase"
STAGE="$WORK_DIR/$ARTIFACT_NAME"
BINSTAGE="$STAGE$BINTARGET"
LIBSTAGE="$STAGE$LIBTARGET"
LIBPLUGINSTAGE="$STAGE$LIBPLUGINTARGET"
LIBPLUGINSTAGE_OPTIONAL="$STAGE$LIBPLUGINTARGET_OPTIONAL"
SHARESTAGE="$STAGE$SHARETARGET"
ETCSTAGE="$STAGE$ETCTARGET"
INITSTAGE="$STAGE$INITTARGET"
INITDSTAGE="$STAGE$INITDTARGET"
SYSTEMDSTAGE="$STAGE$SYSTEMDTARGET"

SERVER_BIN_PATH="$BUILD_DIR/bin"
SERVER_SHARE_PATH="$BUILD_DIR/share"
SERVER_DEB_PATH="$BUILD_DIR/deb"
SERVER_VOX_PATH="$SERVER_BIN_PATH/vox"
SERVER_LIB_PATH="$BUILD_DIR/lib"
SERVER_LIB_PLUGIN_PATH="$SERVER_BIN_PATH/plugins"
SERVER_LIB_PLUGIN_OPTIONAL_PATH="$SERVER_BIN_PATH/plugins_optional"
BUILD_INFO_TXT="$BUILD_DIR/build_info.txt"
LOG_FILE="$LOGS_DIR/server-build-distribution.log"

buildDistribution()
{
    echo "Creating directories"
    mkdir -p "$BINSTAGE"
    mkdir -p "$LIBSTAGE"
    mkdir -p "$LIBPLUGINSTAGE"
    mkdir -p "$LIBPLUGINSTAGE_OPTIONAL"
    mkdir -p "$ETCSTAGE"
    mkdir -p "$SHARESTAGE"
    mkdir -p "$INITSTAGE"
    mkdir -p "$INITDSTAGE"
    mkdir -p "$SYSTEMDSTAGE"

    echo "Copying build_info.txt"
    cp -r "$BUILD_INFO_TXT" "$BINSTAGE/../"

    if [ "$ARCH" != "arm" ]
    then
        echo "Copying dbsync 2.2"
        cp -r "$PACKAGES_DIR/appserver-2.2.1/share/dbsync-2.2" "$SHARESTAGE/"
        cp "$BUILD_DIR/version.py" "$SHARESTAGE/dbsync-2.2/bin/"
    fi

    local LIB
    local LIB_BASENAME
    for LIB in "$SERVER_LIB_PATH"/*.so*
    do
        LIB_BASENAME=$(basename "$LIB")
        if [[ "$LIB_BASENAME" != libQt5* \
            && "$LIB_BASENAME" != libEnginio.so* \
            && "$LIB_BASENAME" != libqgsttools_p.* \
            && "$LIB_BASENAME" != libtegra_video.* \
            && "$LIB_BASENAME" != libnx_client* ]]
        then
            echo "Copying $LIB_BASENAME"
            cp -P "$LIB" "$LIBSTAGE/"
        fi
    done

    # Copy mediaserver plugins.

    local PLUGINS=(
        generic_multicast_plugin
        genericrtspplugin
        it930x_plugin
        mjpg_link
    )
    PLUGINS+=(
        hikvision_metadata_plugin
        axis_metadata_plugin
        dw_mtt_metadata_plugin
        vca_metadata_plugin
    )
    if [ "$ENABLE_HANWHA" == "true" ]
    then
        PLUGINS+=( hanwha_metadata_plugin )
    fi

    local PLUGINS_OPTIONAL=(
        stub_metadata_plugin
    )

    local PLUGIN_FILENAME
    local PLUGIN

    for PLUGIN in "${PLUGINS[@]}"
    do
        PLUGIN_FILENAME="lib$PLUGIN.so"
        echo "Copying (plugin) $PLUGIN_FILENAME"
        cp "$SERVER_LIB_PLUGIN_PATH/$PLUGIN_FILENAME" "$LIBPLUGINSTAGE/"
    done

    for PLUGIN in "${PLUGINS_OPTIONAL[@]}"
    do
        PLUGIN_FILENAME="lib$PLUGIN.so"
        echo "Copying (optional plugin) $PLUGIN_FILENAME"
        cp "$SERVER_LIB_PLUGIN_OPTIONAL_PATH/$PLUGIN_FILENAME" "$LIBPLUGINSTAGE_OPTIONAL/"
    done

    echo "Copying Festival VOX files"
    cp -r $SERVER_VOX_PATH $BINSTAGE

    distr_cpSysLib "$LIBSTAGE" "${CPP_RUNTIME_LIBS[@]}"

    if [ "$ARCH" != "arm" ]
    then
        echo "Copying libicu"
        cp -P "$QT_DIR/lib"/libicu*.so* "$LIBSTAGE/"
    fi

    local QT_LIBS=(
        Core
        Gui
        Xml
        XmlPatterns
        Concurrent
        Network
        Sql
        WebSockets
    )
    local QT_LIBS
    for QT_LIB in "${QT_LIBS[@]}"
    do
        FILE="libQt5$QT_LIB.so"
        echo "Copying (Qt) $FILE"
        cp -P "$QT_DIR/lib/$FILE"* "$LIBSTAGE/"
    done

    # Strip and remove rpath.
    if [[ "$BUILD_CONFIG" == "Release" && "$ARCH" != "arm" ]]
    then
        for FILE in $(find "$LIBPLUGINSTAGE" -type f)
        do
            echo "Stripping $FILE"
            strip "$FILE"
        done

        for FILE in $(find "$LIBSTAGE" -type f)
        do
            echo "Stripping $FILE"
            strip "$FILE"
        done
    fi

    echo "Setting permissions"
    # TODO: Investigate whether settings permissions for the entire build dir contents is ok.
    find -type d -print0 |xargs -0 chmod 755
    find -type f -print0 |xargs -0 chmod 644
    chmod -R 755 "$BINSTAGE"
    if [ "$ARCH" != "arm" ]
    then
        chmod 755 "$SHARESTAGE/dbsync-2.2/bin"/{dbsync,certgen}
    fi

    echo "Copying mediaserver binaries and scripts"
    install -m 755 "$SERVER_BIN_PATH/mediaserver" "$BINSTAGE/mediaserver-bin"
    # The line below will be uncommented when all root_tool related tasks are finished.
    # install -m 750 "$SERVER_BIN_PATH/root_tool" "$BINSTAGE/"
    install -m 755 "$SERVER_BIN_PATH/testcamera" "$BINSTAGE/"
    install -m 755 "$SERVER_BIN_PATH/external.dat" "$BINSTAGE/"
    install -m 755 "$SCRIPTS_DIR/config_helper.py" "$BINSTAGE/"
    install -m 755 "$SCRIPTS_DIR/shell_utils.sh" "$BINSTAGE/"

    echo "Copying mediaserver startup script"
    install -m 755 "bin/mediaserver" "$BINSTAGE/"

    echo "Copying upstart and sysv script"
    install -m 644 "init/networkoptix-mediaserver.conf" \
        "$INITSTAGE/$CUSTOMIZATION-mediaserver.conf"
    install -m 755 "init.d/networkoptix-mediaserver" \
        "$INITDSTAGE/$CUSTOMIZATION-mediaserver"
    install -m 644 "systemd/networkoptix-mediaserver.service" \
        "$SYSTEMDSTAGE/$CUSTOMIZATION-mediaserver.service"

    echo "Preparing DEBIAN dir"
    mkdir -p "$STAGE/DEBIAN"
    chmod 00775 "$STAGE/DEBIAN"

    INSTALLED_SIZE=$(du -s "$STAGE" |awk '{print $1;}')

    echo "Generating DEBIAN/control"
    cat debian/control.template \
        |sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" \
        |sed "s/VERSION/$RELEASE_VERSION/g" \
        |sed "s/ARCHITECTURE/$ARCHITECTURE/g" \
        >"$STAGE/DEBIAN/control"

    echo "Copying DEBIAN dir files"
    install -m 755 "debian/preinst" "$STAGE/DEBIAN/"
    install -m 755 "debian/postinst" "$STAGE/DEBIAN/"
    install -m 755 "debian/prerm" "$STAGE/DEBIAN/"
    install -m 644 "debian/templates" "$STAGE/DEBIAN/"

    echo "Generating DEBIAN/md5sums"
    (cd "$STAGE"
        find * -type f -not -regex '^DEBIAN/.*' -print0 |xargs -0 md5sum >"DEBIAN/md5sums"
        chmod 644 "DEBIAN/md5sums"
    )

    echo "Creating .deb $ARTIFACT_NAME.deb"
    (cd "$WORK_DIR"
        fakeroot dpkg-deb -b "$ARTIFACT_NAME"
    )

    local DEB
    for DEB in "$SERVER_DEB_PATH"/*
    do
        echo "Copying $DEB"
        cp -Rf "$DEB" "$WORK_DIR/"
    done

    echo "Generating ARTIFACT_NAME-server.properties"
    echo "server.ARTIFACT_NAME=$ARTIFACT_NAME" >> ARTIFACT_NAME-server.properties

    # Copying filtered resources.
    local RESOURCE
    for RESOURCE in "deb"/*
    do
        echo "Copying filtered $(basename "$RESOURCE")"
        cp -r "$RESOURCE" "$WORK_DIR/"
    done

    echo "Creating .zip $UPDATE_NAME"
    (cd "$WORK_DIR"
        zip -r "./$UPDATE_NAME" ./ -x "./$ARTIFACT_NAME"/**\* "$ARTIFACT_NAME/"
    )

    if [ ! -z "$DISTRIBUTION_OUTPUT_DIR" ]
    then
        echo "Moving distribution .zip and .deb to $DISTRIBUTION_OUTPUT_DIR"
        mv "$WORK_DIR/$UPDATE_NAME" "$DISTRIBUTION_OUTPUT_DIR/"
        mv "$WORK_DIR/$ARTIFACT_NAME.deb" "$DISTRIBUTION_OUTPUT_DIR/"
    else
        echo "Moving distribution .zip to $CURRENT_BUILD_DIR"
        mv "$WORK_DIR/$UPDATE_NAME" "$CURRENT_BUILD_DIR/"
    fi
}

main()
{
    distr_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
