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

    if [[ "$BUILD_CONFIG" == "Release" && "$ARCH" != "aarch64" ]]
    then
        local FILE
        for FILE in $(find "$DIR" -type f)
        do
            echo "  Stripping $FILE"
            # `strip` does not print any errors to stderr, just returns a non-zero status.
            local -i STRIP_STATUS=0
            strip "$FILE" || STRIP_STATUS=$?
            if [ $STRIP_STATUS != 0 ]
            then
                echo "\`strip\` failed (status $STRIP_STATUS) for $FILE" >&2
                exit $STRIP_STATUS
            fi
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
    local BLACKLIST_ITEM
    local SKIP_LIBRARY

    local -r LIB_BLACKLIST=(
        'libQt5*'
        'libEnginio.so*'
        'libqgsttools_p.*'
        'libtegra_video.*'
        'libnx_vms_client*'
        'libcloud_db.*'
    )

    for LIB in "$BUILD_DIR/lib"/*.so*
    do
        LIB_BASENAME=$(basename "$LIB")

        SKIP_LIBRARY=0

        for BLACKLIST_ITEM in "${LIB_BLACKLIST[@]}"; do
            if [[ $LIB_BASENAME == $BLACKLIST_ITEM ]]; then
                SKIP_LIBRARY=1
                break
            fi
        done

        (( $SKIP_LIBRARY == 1 )) && continue

        if [[ "$LIB_BASENAME" == libtegra_video.* ]]; then
            [[ "$BOX" != "tx1" ]] && continue
        fi

        echo "  Copying $LIB_BASENAME"
        cp -P "$LIB" "$STAGE_LIB/"
    done

    echo "  Copying system libs: ${CPP_RUNTIME_LIBS[@]}"
    distrib_copySystemLibs "$STAGE_LIB" "${CPP_RUNTIME_LIBS[@]}"
    if [[ "$ARCH" != "arm" ]]
    then
        echo "Copying libicu"
        distrib_copySystemLibs "$STAGE_LIB" "${ICU_RUNTIME_LIBS[@]}"
    fi

    stripIfNeeded "$STAGE_LIB"
}

# [in] STAGE_MODULE
copyMediaserverPlugins()
{
    echo ""
    echo "Copying mediaserver plugins"

    distrib_copyMediaserverPlugins "plugins" "$STAGE_MODULE/bin" "${SERVER_PLUGINS[@]}"
    stripIfNeeded "$STAGE_MODULE/bin/plugins"

    distrib_copyMediaserverPlugins "plugins_optional" "$STAGE_MODULE/bin" \
        "${SERVER_PLUGINS_OPTIONAL[@]}"
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
# [in] STAGE_QT_PLUGINS
copyQtLibs()
{
    echo ""
    echo "Copying Qt libs"

    local QT_LIBS=(
        Core
        Gui
        Xml
        XmlPatterns
        Concurrent
        Network
        Sql
        WebSockets
        Qml
    )
    local QT_LIBS
    for QT_LIB in "${QT_LIBS[@]}"
    do
        FILE="libQt5$QT_LIB.so"
        echo "  Copying (Qt) $FILE"
        cp -P "$QT_DIR/lib/$FILE"* "$STAGE_LIB/"
    done

    local -r QT_PLUGINS=(
        sqldrivers/libqsqlite.so
    )

    for PLUGIN in "${QT_PLUGINS[@]}"
    do
        echo "Copying (Qt plugin) $PLUGIN"

        mkdir -p "$STAGE_QT_PLUGINS/$(dirname $PLUGIN)"
        cp -r "$QT_DIR/plugins/$PLUGIN" "$STAGE_QT_PLUGINS/$PLUGIN"
    done
}

# [in] STAGE_BIN
copyBins()
{
    echo "Copying mediaserver binaries and scripts"
    install -m 755 "$BUILD_DIR/bin/mediaserver" "$STAGE_BIN/mediaserver-bin"
    if [ "$ENABLE_ROOT_TOOL" = "true" ]
    then
        install -m 750 "$BUILD_DIR/bin/root_tool" "$STAGE_BIN/root-tool-bin"
    fi
    install -m 755 "$BUILD_DIR/bin/testcamera" "$STAGE_BIN/"
    install -m 755 "$BUILD_DIR/bin/external.dat" "$STAGE_BIN/" #< TODO: Why "+x" is needed?
    install -m 644 "$CURRENT_BUILD_DIR/qt.conf" "$STAGE_BIN/"
    install -m 755 "$SCRIPTS_DIR/config_helper.py" "$STAGE_BIN/"
    install -m 755 "$SCRIPTS_DIR/shell_utils.sh" "$STAGE_BIN/"

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
    install -m 644 "init/networkoptix-root-tool.conf" \
        "$STAGE/etc/init/$CUSTOMIZATION-root-tool.conf"

    install -m 755 -d "$STAGE/etc/init.d"
    install -m 755 "init.d/networkoptix-mediaserver" \
        "$STAGE/etc/init.d/$CUSTOMIZATION-mediaserver"
    install -m 755 "init.d/networkoptix-root-tool" \
        "$STAGE/etc/init.d/$CUSTOMIZATION-root-tool"

    install -m 755 -d "$STAGE/etc/systemd/system"
    install -m 644 "systemd/networkoptix-mediaserver.service" \
        "$STAGE/etc/systemd/system/$CUSTOMIZATION-mediaserver.service"
    install -m 644 "systemd/networkoptix-root-tool.service" \
        "$STAGE/etc/systemd/system/$CUSTOMIZATION-root-tool.service"
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
    local -r STAGE_QT_PLUGINS="$STAGE_MODULE/plugins"

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
