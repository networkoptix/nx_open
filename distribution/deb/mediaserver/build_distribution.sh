#!/bin/bash
set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="server_build_distribution_tmp"
LOG_FILE="$LOGS_DIR/server_build_distribution.log"

# Strip binaries and remove rpath.
stripIfNeeded() # dir
{
    local -r DIR="$1" && shift

    if [[ "$BUILD_CONFIG" == "Release" && "$ARCH" != "arm" ]]
    then
        local FILE
        for FILE in $(find "$DIR" -type f)
        do
            echo "  Stripping $FILE"
            strip "$FILE"
        done
    fi
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
            && "$LIB_BASENAME" != libnx_client* ]]
        then
            echo "  Copying $LIB_BASENAME"
            cp -P "$LIB" "$STAGE_LIB/"
        fi
    done

    echo "  Copying system libs: ${CPP_RUNTIME_LIBS[@]}"
    distrib_copySystemLibs "$STAGE_LIB" "${CPP_RUNTIME_LIBS[@]}"

    stripIfNeeded "$STAGE_LIB"
}

# [in] STAGE_MODULE
copyMediaserverPlugins()
{
    echo ""
    echo "Copying mediaserver plugins"

    local PLUGINS=(
        generic_multicast_plugin
        genericrtspplugin
        it930x_plugin
        mjpg_link
    )
    PLUGINS+=( # Metadata plugins.
        hikvision_metadata_plugin
        axis_metadata_plugin
        dw_mtt_metadata_plugin
        vca_metadata_plugin
    )
    if [ "$ENABLE_HANWHA" == "true" ]
    then
        PLUGINS+=( hanwha_metadata_plugin )
    fi

    distrib_copyMediaserverPlugins "plugins" "$STAGE_MODULE/bin" "${PLUGINS[@]}"
    stripIfNeeded "$STAGE_MODULE/bin/plugins"

    local PLUGINS_OPTIONAL=(
        stub_metadata_plugin
    )

    distrib_copyMediaserverPlugins "plugins_optional" "$STAGE_MODULE/bin" "${PLUGINS_OPTIONAL[@]}"
    stripIfNeeded "$STAGE_MODULE/bin/plugins_optional"
}

# [in] STAGE_BIN
copyFestivalVox()
{
    echo "Copying Festival Vox files"
    mkdir -p "$STAGE_BIN"
    cp -r "$BUILD_DIR/bin/vox" "$STAGE_BIN/"
}

# [in] STAGE_LIB
copyQtLibs()
{
    echo ""
    echo "Copying Qt libs"

    if [ "$ARCH" != "arm" ]
    then
        echo "  Copying (Qt) libicu"
        cp -P "$QT_DIR/lib"/libicu*.so* "$STAGE_LIB/"
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
        echo "  Copying (Qt) $FILE"
        cp -P "$QT_DIR/lib/$FILE"* "$STAGE_LIB/"
    done
}

# [in] STAGE_SHARE
# [in] STAGE_MODULE
copyDbSyncIfNeeded()
{
    if [ "$ARCH" != "arm" ]
    then
        echo ""
        echo "Copying dbsync 2.2"
        local -r STAGE_SHARE="$STAGE_MODULE/share"
        mkdir -p "$STAGE_SHARE"
        cp -r "$PLATFORM_PACKAGES_DIR/appserver-2.2.1/share/dbsync-2.2" "$STAGE_SHARE/"
        cp "$BUILD_DIR/version.py" "$STAGE_SHARE/dbsync-2.2/bin/"
        find "$STAGE_SHARE" -type d -print0 |xargs -0 chmod 755
        find "$STAGE_SHARE" -type f -print0 |xargs -0 chmod 644
        chmod 755 "$STAGE_SHARE/dbsync-2.2/bin"/{dbsync,certgen}
    fi
}

# [in] STAGE_BIN
copyBins()
{
    echo "Copying mediaserver binaries and scripts"
    install -m 755 "$BUILD_DIR/bin/mediaserver" "$STAGE_BIN/mediaserver-bin"
    # The line below will be uncommented when all root_tool related tasks are finished.
    # install -m 750 "$BUILD_DIR/bin/root_tool" "$STAGE_BIN/"
    install -m 755 "$BUILD_DIR/bin/testcamera" "$STAGE_BIN/"
    install -m 755 "$BUILD_DIR/bin/external.dat" "$STAGE_BIN/" #< TODO: Why "+x" is needed?
    install -m 755 "$SCRIPTS_DIR/config_helper.py" "$STAGE_BIN/"
    install -m 755 "$SCRIPTS_DIR/shell_utils.sh" "$STAGE_BIN/"
    install -m 755 "$SOURCE_DIR/nx_log_viewer.html" "$STAGE_BIN/"

    echo "Copying mediaserver startup script"
    install -m 755 "bin/mediaserver" "$STAGE_BIN/"
}

# [in] STAGE
copyStartupScripts()
{
    echo "Copying upstart and sysv script"
    install -m 755 -d "$STAGE/etc/init"
    install -m 644 "init/networkoptix-mediaserver.conf" \
        "$STAGE/etc/init/$CUSTOMIZATION-mediaserver.conf"
    install -m 755 -d "$STAGE/etc/init.d"
    install -m 755 "init.d/networkoptix-mediaserver" \
        "$STAGE/etc/init.d/$CUSTOMIZATION-mediaserver"
    install -m 755 -d "$STAGE/etc/systemd/system"
    install -m 644 "systemd/networkoptix-mediaserver.service" \
        "$STAGE/etc/systemd/system/$CUSTOMIZATION-mediaserver.service"
}

# [in] STAGE
createDebianDir()
{
    echo "Preparing DEBIAN dir"
    mkdir -p "$STAGE/DEBIAN"
    chmod 00775 "$STAGE/DEBIAN"

    local -r INSTALLED_SIZE=$(du -s "$STAGE" |awk '{print $1;}')

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
}

# [in] WORK_DIR
createUpdateZip() # file.deb
{
    local -r DEB_FILE="$1" && shift

    echo ""
    echo "Creating \"update\" .zip"

    local -r ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"

    echo "  Creating symlink to .deb"
    ln -s "$DEB_FILE" "$ZIP_DIR/"

    local DEB
    for DEB in "$BUILD_DIR/deb"/*
    do
        echo "  Copying $(basename "$DEB")"
        cp -r "$DEB" "$ZIP_DIR/"
    done

    local FILE
    for FILE in "update"/*
    do
        echo "  Copying (configured) $(basename "$FILE")"
        cp -r "$FILE" "$ZIP_DIR/"
    done

    distrib_createArchive "$DISTRIBUTION_OUTPUT_DIR/$UPDATE_ZIP" "$ZIP_DIR" zip -r
}

# [in] WORK_DIR
buildDistribution()
{
    local -r STAGE="$WORK_DIR/$DISTRIBUTION_NAME"
    local -r STAGE_MODULE="$STAGE/opt/$CUSTOMIZATION/mediaserver"
    local -r STAGE_BIN="$STAGE_MODULE/bin"
    local -r STAGE_LIB="$STAGE_MODULE/lib"

    echo "Creating stage dir $STAGE_MODULE/etc"
    mkdir -p "$STAGE_MODULE/etc" #< TODO: This folder is always empty. Is it needed?

    echo "Copying build_info.txt"
    mkdir -p "$STAGE_MODULE/"
    cp "$BUILD_DIR/build_info.txt" "$STAGE_MODULE/"
    cp "$BUILD_DIR/specific_features.txt" "$STAGE_MODULE/"

    copyLibs
    copyQtLibs
    copyMediaserverPlugins
    copyFestivalVox

    echo "Setting permissions"
    find "$STAGE" -type d -print0 |xargs -0 chmod 755
    find "$STAGE" -type f -print0 |xargs -0 chmod 644
    chmod -R 755 "$STAGE_BIN" #< Restore executable permission for the files in "bin" recursively.

    copyDbSyncIfNeeded
    copyBins
    copyStartupScripts

    createDebianDir

    local -r DEB="$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_NAME.deb"

    echo "Creating $DEB"
    fakeroot dpkg-deb -b "$STAGE" "$DEB"

    createUpdateZip "$DEB"

    # TODO: This file seems to go nowhere. Is it needed? If yes, add a comment.
    echo "Generating finalname-server.properties"
    echo "server.finalName=$DISTRIBUTION_NAME" >>finalname-server.properties
}

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
