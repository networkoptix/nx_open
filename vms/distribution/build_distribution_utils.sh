#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Utilities helpful for creating distribution packages (.deb, .dmg or archives).
#
# All global symbols have prefix "distrib_".

#--------------------------------------------------------------------------------------------------
# private

# [in] distrib_EXTRA_HELP
#
distrib_showHelp()
{
    echo "Options:"
    echo " -v, --verbose: Do not redirect output to a log file."
    echo " -k, --keep-work-dir: Do not delete work directory on success."
    if [[ ! -z $distrib_EXTRA_HELP ]]
    then
        echo "$distrib_EXTRA_HELP"
    fi
}

# [out] distrib_KEEP_WORK_DIR
# [out] distrib_VERBOSE
#
distrib_parseArgs() # "$@"
{
    distrib_KEEP_WORK_DIR=0
    distrib_VERBOSE=0

    local -i ARG_N=0
    local ARG
    for ARG in "$@"
    do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]
        then
            distrib_showHelp
            exit 0
        elif [ "$ARG" = "-k" ] || [ "$ARG" = "--keep-work-dir" ]
        then
            distrib_KEEP_WORK_DIR=1
        elif [ "$ARG" = "-v" ] || [ "$ARG" = "--verbose" ]
        then
            distrib_VERBOSE=1
        elif [[ ! -z $distrib_PARSE_EXTRA_ARGS_FUNC ]]
        then
            $distrib_PARSE_EXTRA_ARGS_FUNC "$ARG" $ARG_N "$@"
        fi

        ((++ARG_N))
    done
}

# [out] distrib_STDERR_FILE
#
distrib_redirectOutputToLog() # log_file
{
    local -r LOG_FILE="$1" && shift

    echo "  See the log in $LOG_FILE"

    mkdir -p "$(dirname "$LOG_FILE")" #< In case log dir is not created yet.
    rm -rf "$LOG_FILE"

    # Redirect only stdout to the log file, leaving stderr as is, because there seems to be no
    # reliable way to duplicate error output to both log file and stderr, keeping the correct order
    # of regular and error lines in the log file.
    exec 1>"$LOG_FILE" #< Open log file for writing as stdout.

    distrib_STDERR_FILE=$(mktemp)
    exec 2> >(tee "$distrib_STDERR_FILE" >&2) #< Redirect stderr to a temporary file.
}

# Called by trap.
# [in] $?
# [in] distrib_VERBOSE
# [in] distrib_STDERR_FILE
# [in] distrib_KEEP_WORK_DIR
# [in] distrib_WORK_DIR
#
distrib_onExit()
{
    local -r -i RESULT=$?

    set +x #< Disable logging each command (in case it was enabled previously with "set -x").

    if [ -s "$distrib_STDERR_FILE" ] #< Whether the file exists and is not empty.
    then
        echo ""
        python -c 'print("-" * 99)' #< Print separator.
        echo "- stderr"
        echo ""
        cat "$distrib_STDERR_FILE" #< Copy accumulated stderr to the log file.
    fi
    rm -rf "$distrib_STDERR_FILE"

    echo "" #< Empty line to separate SUCCESS/FAILURE message from the above log.
    if [ $RESULT != 0 ]
    then #< Failure.
        local -r ERROR_MESSAGE="FAILURE (status $RESULT); see the error message(s) above."
        echo "$ERROR_MESSAGE" >&2 #< To stderr.
        if [ $distrib_VERBOSE = 0 ] #< Whether stdout is redirected to the log file.
        then
            echo "$ERROR_MESSAGE" #< To log file.
        fi
    else
        echo "SUCCESS"

        if [ $distrib_KEEP_WORK_DIR = 0 ]
        then
            rm -rf "$distrib_WORK_DIR"
        fi
    fi

    return $RESULT
}

# Copy specified plugins from $BUILD_DIR/bin/plugins-dir-name to target-dir/plugins-dir-name>/.
# Create target-dir if needed.
#
# [in] BUILD_DIR
#
distrib_copyMediaserverPluginsToDir() # target_dir_name plugin_lib_name...
{
    local -r plugins_dir_name="$1" && shift #< E.g. plugins, plugins_optional.

    if (( $# == 0 )); then
        return
    fi

    local -r target_dir="${STAGE}/${BIN_INSTALL_PATH}/${plugins_dir_name}"
    mkdir -p "${target_dir}"

    local plugin
    local plugin_path
    for plugin in "$@"; do
        if [[ -f "${plugin}" ]]; then
            # File name instead of the plugin name was passed.
            plugin_path="${plugin}"
        elif [[ -d "${BUILD_DIR}/bin/${plugins_dir_name}/${plugin}" ]]; then
            # Plugin is a "directory" plugin.
            plugin_path="${BUILD_DIR}/bin/${plugins_dir_name}/${plugin}"
        else
            plugin_path="${BUILD_DIR}/bin/${plugins_dir_name}/lib${plugin}.so"
        fi

        if [[ -f "${plugin_path}" ]]; then
            echo "  Copying ${plugin_path} to ${plugins_dir_name}"
            cp "${plugin_path}" "${target_dir}/"
        elif [[ -d "${plugin_path}" ]]; then
            echo "  Copying ${plugin} dedicated directory to ${plugins_dir_name}"
            cp -r "${plugin_path}" "${target_dir}/"
        else
            echo "ERROR: Cannot find plugin ${plugin}" >&2
        fi
    done
}

# In the specified directory and all subdirectories recursively, strips from debug symbols all .so
# and .so.<version> files, and files from additional_file_names (must be specified without paths).
distrib_stripDirectory() # directory [additional_file_names...]
{
    local -r dir="$1" && shift;
    local -a additional_file_names=("$@")

    local -a files
    readarray -d "" files < <(find "${dir}"  -type f -name "*.so*" -print0)
    for file_name in "${additional_file_names[@]}"; do
        local -a additional_files
        readarray -d "" additional_files < <(find "${dir}" -type f -name "${file_name}" -print0)
        files+=("${additional_files[@]}")
    done

    local file
    for file in "${files[@]}"; do
        if [[ -z "${file}" ]]; then
            continue
        fi

        echo "  Stripping ${file}"
        local -i strip_status=0
        "${STRIP}" "${file}" || strip_status="$?"
        if [[ "${strip_status}" != 0 ]]; then
            echo "\"strip\" failed (status ${strip_status}) for ${file}" >&2
            exit "${strip_status}"
        fi
    done
}

# Global variables - need to be accessible in onExit().
declare distrib_WORK_DIR
declare -i distrib_VERBOSE
declare -i distrib_KEEP_WORK_DIR
declare distrib_STDERR_FILE

#--------------------------------------------------------------------------------------------------
# public

# Description of additional command-line options to show with the help.
declare distrib_EXTRA_HELP=""

# Callback which is called for each command-line argument:
#     parseExtraArgs arg_value arg_number "$@"
declare distrib_PARSE_EXTRA_ARGS_FUNC=""

# [in,out] CONFIG If defined, overrides config_file, otherwise is set to config_file.
#
distrib_loadConfig() # config_file
{
    : ${CONFIG="$1"} && shift #< CONFIG can be overridden externally.
    if [ ! -z "$CONFIG" ]
    then
        source "$CONFIG"
    fi
}

# Read installed size from a DEB package.
#
# dpkg-deb will print the required info like "Installed-Size: 12345" where 12345 is approximate
# size in kylobytes.
#
# [in] DEB_FILE
#
distrib_getInstalledSizeFromDeb() # deb_file
{
    local -r DEB_FILE="$1" && shift

    local FREE_SPACE_REQUIRED=$(dpkg-deb -I "$DEB_FILE" |grep "Installed-Size:" |awk '{print $2}')
    FREE_SPACE_REQUIRED=$(($FREE_SPACE_REQUIRED * 1024))
    echo $FREE_SPACE_REQUIRED
}

# Copy system libraries using copy_system_library.py.
#
# [in] OPEN_SOURCE_DIR
# [in] COMPILER
# [in] CFLAGS
# [in] LDFLAGS
#
distrib_copySystemLibs() # dest_dir libs_to_copy...
{
    local -r DEST_DIR="$1" && shift

    "$PYTHON" "$OPEN_SOURCE_DIR"/build_utils/linux/copy_system_library.py \
        --compiler="$COMPILER" \
        --flags="$CFLAGS" \
        --link-flags="$LDFLAGS" \
        --dest-dir="$DEST_DIR" \
        "$@"
}

# Copy all system libraries needed by the Server.
#
# [in] STAGE
# [in] CPP_RUNTIME_LIBS
# [in] ICU_RUNTIME_LIBS
#
distrib_copyServerSystemLibs() # destination_path
{
    local -r destination_path="$1"

    echo ""
    echo "Copying system libs"

    echo "Copying cpp runtime libs: ${CPP_RUNTIME_LIBS[@]}"
    distrib_copySystemLibs "${destination_path}" "${CPP_RUNTIME_LIBS[@]}"

    echo "Copying libicu"
    distrib_copySystemLibs "${destination_path}" "${ICU_RUNTIME_LIBS[@]}"

    echo "Copying ALSA library (used only if libasound2 deb is not installed)"

    mkdir -p "${destination_path}/libasound2"
    distrib_copySystemLibs "${destination_path}/libasound2" libasound.so.2
}

# Copy libraries from the specified directory using copy_system_library.py.
#
# [in] OPEN_SOURCE_DIR
# [in] LIB_DIR
#
distrib_copyLibs() # lib_dir dest_dir libs_to_copy...
{
    local -r LIB_DIR="$1" && shift
    local -r DEST_DIR="$1" && shift

    "$PYTHON" "$OPEN_SOURCE_DIR"/build_utils/linux/copy_system_library.py \
        -L "$LIB_DIR" \
        --dest-dir="$DEST_DIR" \
        "$@"
}

# Copy the specified file to the proper location, creating the symlink if necessary: if
# ALT_LIB_INSTALL_PATH is unset, or the file is a symlink, just copy it to STAGE/LIB_INSTALL_PATH;
# otherwise, put it to STAGE/ALT_LIB_INSTALL_PATH, and create a symlink from
# STAGE/LIB_INSTALL_PATH to the file basename in INSTALL_ROOT/ALT_LIB_INSTALL_PATH. If an optional
# parameter library_subdirectory is passed to the function, it adds it as a suffix to both
# LIB_INSTALL_PATH and ALT_LIB_INSTALL_PATH.
#
# [in] STAGE
# [in] INSTALL_ROOT
# [in] LIB_INSTALL_PATH
# [in] ALT_LIB_INSTALL_PATH Path to the alternative lib directory in the distribution creation
#      directory.
distrib_copyLib() # file [library_subdirectory]
{
    local -r file="$1"
    local -r lib_subdir="${2-}" #< The default value for lib_subdir is an empty string.

    # Create a temporary variable which value is the same as the value of the
    # ALT_LIB_INSTAL_PATH variable if it is set, or empty otherwise.
    local -r alt_lib_instal_path_local="${ALT_LIB_INSTALL_PATH-}"

    if [[ -z "${lib_subdir}" ]]; then
        local -r lib_dir="${LIB_INSTALL_PATH}"
        local -r alt_lib_dir="${alt_lib_instal_path_local}"
    else
        local -r lib_dir="${LIB_INSTALL_PATH}/${lib_subdir}"
        local -r alt_lib_dir="${alt_lib_instal_path_local}/${lib_subdir}"
    fi

    local -r stage_lib="${STAGE}/${lib_dir}"

    if [[ ! -z "${alt_lib_instal_path_local}" && ! -L "${file}" ]]; then
        # FILE is not a symlink - put to the alt location, and create a symlink to it.
        cp -r "${file}" "${STAGE}/${alt_lib_dir}/"
        local -r file_name=$(basename "${file}")
        local -r install_root="${INSTALL_ROOT-}"
        ln -s "${install_root}/${alt_lib_dir}/${file_name}" "${stage_lib}/${file_name}"
    else
        # FILE is a symlink, or the alt location is not defined - put to the regular location.
        cp -r "${file}" "${stage_lib}/"
    fi
}

# Copy Server libs to the distribution creation folder using distrib_copyLib() to ensure the
# correct directory structure (libraries and symlinks to them in different directories if
# necessary).
#
# ATTENTION: We explicitly add all the necessary libraries to the list, so this function is
# version-specific and must be reviewed every time when a new library is added to the Server
# distribution.
#
# [in] BUILD_DIR
# [in] STAGE
# [in] LIB_INSTALL_PATH
# [in] ALT_LIB_INSTALL_PATH
distrib_copyServerLibs() # additional_libs_to_copy...
{
    local -a additional_server_libs=("$@")

    echo ""
    echo "Copying libs"

    local -a libs_to_copy=(
        # vms
        libappserver2
        libcloud_db_client
        libnx_vms_server
        libnx_vms_server_db
        libnx_speech_synthesizer
        libnx_email
        libnx_fusion
        libnx_reflect
        libnx_kit
        libnx_branding
        libnx_build_info
        libnx_monitoring
        libnx_network
        libnx_utils
        libnx_vms_utils
        libnx_zip
        libnx_sql
        libnx_vms_api
        libnx_analytics_db
        libnx_vms_common
        libnx_vms_rules
        libnx_vms_update
        libonvif
        libnx_onvif
        libnx_ldap
        libnx_vms_json_rpc

        # third-party
        libquazip
        libudt
        liblber-2.4
        libldap_r-2.4

        # additional libraries
        "${additional_server_libs[@]}"
    )

    local -r optional_libs_to_copy=(
        libvpx
    )

    # OpenSSL.
    libs_to_copy+=(
        libssl
        libcrypto
    )

    mkdir -p "${STAGE}/${LIB_INSTALL_PATH}"
    # if ALT_LIB_INSTALL_PATH is not set, consider it to be an empty string.
    if [[ ! -z "${ALT_LIB_INSTALL_PATH-}" ]]; then
        mkdir -p "${STAGE}/${ALT_LIB_INSTALL_PATH}"
    fi

    local lib
    for lib in "${libs_to_copy[@]}"; do
        local file
        for file in "${BUILD_DIR}/lib/${lib}".so*; do
            if [[ "${file}" != *.debug ]]; then
                echo "  Copying $(basename "${file}")"
                distrib_copyLib "${file}"
            fi
        done
    done
    for lib in "${optional_libs_to_copy[@]}"; do
        local file
        for file in "${BUILD_DIR}/lib/${lib}".so*; do
            if [[ -f "${file}" ]]; then
                echo "  Copying (optional) $(basename "${file}")"
                distrib_copyLib "${file}"
            fi
        done
    done
}

# Copy ffmpeg libs to the distribution creation directory.
#
# [in] STAGE
# [in] LIB_INSTALL_PATH
# [in] ALT_LIB_INSTALL_PATH
distrib_copyFfmpegLibs() # source_directory [distribution_libs_ffmpeg_subdirectory]
{
    local -r source_directory="$1"
    local -r ffmpeg_subdirectory="${2-}" #< The default value for this argument is an empty string.

    local -r ffmpeg_libs=(
        libavcodec.so.58
        libavfilter.so.7
        libavformat.so.58
        libavutil.so.56
        libswscale.so.5
        libswresample.so.3
        libavdevice.so.58
    )

    echo ""
    echo "Copying FFmpeg libs"

    if [[ ! -z "${ffmpeg_subdirectory}" ]]; then
        mkdir -p "${STAGE}/${LIB_INSTALL_PATH}/${ffmpeg_subdirectory}"
        # if ALT_LIB_INSTALL_PATH is not set, consider it to be an empty string.
        if [[ ! -z "${ALT_LIB_INSTALL_PATH-}" ]]; then
            mkdir -p "${STAGE}/${ALT_LIB_INSTALL_PATH}/${ffmpeg_subdirectory}"
        fi
    fi

    for lib in "${ffmpeg_libs[@]}"; do
        local file
        for file in "${source_directory}/${lib}"*; do
            if [ -f "${file}" ]; then
                echo "  Copying $(basename "${file}")"
                distrib_copyLib "${file}" "${ffmpeg_subdirectory}"
            fi
        done
    done
}

# Copy Qt libs to the distribution creation directory.
#
# [in] QT_DIR
distrib_copyQtLibs() # additional_libs_to_copy...
{
    local -a additional_qt_libs=("$@")

    echo ""
    echo "Copying Qt libs"

    local qt_libs_to_copy=(
        Concurrent
        Core
        Gui
        Xml
        Network
        Sql
        WebSockets
        Qml

        "${additional_qt_libs[@]}"
    )

    local qt_lib
    for qt_lib in "${qt_libs_to_copy[@]}"; do
        local lib_filename="libQt5${qt_lib}.so"
        echo "  Copying (Qt) ${lib_filename}"
        local file
        for file in "${QT_DIR}/lib/${lib_filename}"*; do
            distrib_copyLib "${file}"
        done
    done
}

# Copy Qt plugins to the distribution creation directory.
#
# [in] STAGE
# [in] MODULE_INSTALL_PATH
# [in] QT_DIR
distrib_copyQtPlugins() # additional_plugins_to_copy...
{
    local -a plugins=("$@")

    echo ""
    echo "Copying Qt plugins for mediaserver"

    local -r qt_plugins_install_dir="${STAGE}/${MODULE_INSTALL_PATH}/plugins"
    mkdir -p "${qt_plugins_install_dir}"

    local plugin
    for plugin in "${plugins[@]}"; do
        echo "  Copying (Qt plugin) ${plugin}"

        mkdir -p "${qt_plugins_install_dir}/$(dirname "${plugin}")"
        cp -r "${QT_DIR}/plugins/${plugin}" "${qt_plugins_install_dir}/${plugin}"
    done
}

# Copy libGL libraries
#
# [in] BUILD_DIR
distrib_copyGlLibs()
{
    if [[ "${ARCH}" = "arm" ]]; then
        return
    fi

    echo ""
    echo "Copying LibGL stub"

    local -a libs_to_copy=(
        libGL.so.1
    )

    local lib
    for lib in "${libs_to_copy[@]}"; do
        local file="${BUILD_DIR}/lib/libgl_stub/${lib}"
        echo "  Copying $(basename "${file}")"
        distrib_copyLib "${file}"
    done
}

# Copy server binaries and other files that must reside next to them to the distribution creation
# directory.
#
# [in] STAGE
# [in] INSTALL_ROOT
# [in] BIN_INSTALL_PATH
# [in] LIB_INSTALL_PATH
# [in] BUILD_DIR
# [in] CURRENT_BUILD_DIR
# [in] ENABLE_ROOT_TOOL
# [in] OPEN_SOURCE_DIR
# [in] SERVER_SCRIPTS_DIR
distrib_copyServerBins() # additional_bins_to_copy...
{
    local -a additional_server_binaries=("$@")

    echo ""
    echo "Copying mediaserver binaries"

    local -r stage_bin="${STAGE}/${BIN_INSTALL_PATH}"
    mkdir -p "${stage_bin}"

    install -m 755 "${BUILD_DIR}/bin/mediaserver" "${stage_bin}/"
    install -m 755 "${BUILD_DIR}/bin/external.dat" "${stage_bin}/" #< TODO: Why "+x" is needed?
    for bin in "${additional_server_binaries[@]}"; do
        echo "  Installing ${bin}..."
        install -m 755 "${bin}" "${stage_bin}/"
    done

    echo "Copying translations"
    install -m 755 -d "${stage_bin}/translations"
    install -m 644 "${BUILD_DIR}/bin/translations/nx_vms_common.dat" "${stage_bin}/translations/"
    install -m 644 "${BUILD_DIR}/bin/translations/nx_vms_rules.dat" "${stage_bin}/translations/"
    install -m 644 "${BUILD_DIR}/bin/translations/nx_vms_server.dat" "${stage_bin}/translations/"
    install -m 644 "${BUILD_DIR}/bin/translations/nx_vms_server_db.dat" \
        "${stage_bin}/translations/"

    # if ENABLE_ROOT_TOOL is not set, consider it to be "false".
    if [[ "${ENABLE_ROOT_TOOL-"false"}" == "true" ]]; then
        echo "Copying root-tool"
        install -m 750 "${BUILD_DIR}/bin/root_tool" "${stage_bin}/root-tool"

        # Use the flavor-specific system_commands.conf if it exists; otherwise use the standard
        # one.
        if [[ -f "${CURRENT_BUILD_DIR}/system_commands.conf" ]]; then
            local -r system_commands_conf="${CURRENT_BUILD_DIR}/system_commands.conf"
        else
            local -r system_commands_conf="${BUILD_DIR}/bin/system_commands.conf"
        fi
        install -m 640 "${system_commands_conf}" "${stage_bin}/"

        # Fix root-tool rpath.
        local -r install_root="${INSTALL_ROOT-}"
        chrpath --replace "${install_root}/${LIB_INSTALL_PATH}" "${stage_bin}/root-tool"
    fi

    echo "Copying nx_log_viewer.html"
    install -m 755 "${OPEN_SOURCE_DIR}/nx_log_viewer.html" "${stage_bin}/"

    echo "Copying qt.conf"
    install -m 644 "${CURRENT_BUILD_DIR}/qt.conf" "${stage_bin}/"

    # Copy the Server scripts if the variable SERVER_SCRIPTS_DIR is defined and not empty.
    if [[ ! -z "${SERVER_SCRIPTS_DIR-}" ]]; then
        echo "Copying Server scripts"
        install -m 755 -d "${STAGE}/${SERVER_SCRIPTS_DIR}"
        install -m 755 "${CURRENT_BUILD_DIR}/scripts"/* "${STAGE}/${SERVER_SCRIPTS_DIR}/"
    fi

    echo "Copying configuration scripts"
    install -m 755 "${SCRIPTS_DIR}/config_helper.sh" "${stage_bin}/"
    install -m 755 "${SCRIPTS_DIR}/upgrade_config.sh" "${stage_bin}/"
}

# [in] SERVER_PLUGINS[]
# [in] SERVER_PLUGINS_OPTIONAL[]
# [in] SERVER_PLUGINS_TO_MOVE_TO_OPTIONAL[]
# [in] STAGE
# [in] BIN_INSTALL_PATH
distrib_copyMediaserverPlugins()
{
    echo ""
    echo "Copying Server plugins"

    distrib_copyMediaserverPluginsToDir "plugins" "${SERVER_PLUGINS[@]}"
    distrib_copyMediaserverPluginsToDir "plugins_optional" "${SERVER_PLUGINS_OPTIONAL[@]}"

    local -r main_plugin_dir="${STAGE}/${BIN_INSTALL_PATH}/plugins"
    local -r optional_plugin_dir="${STAGE}/${BIN_INSTALL_PATH}/plugins_optional"
    local plugin
    for plugin in "${SERVER_PLUGINS_TO_MOVE_TO_OPTIONAL[@]}"; do
        if [[ -d "${main_plugin_dir}/${plugin}" ]]; then
            echo "Moving ${plugin} from ${main_plugin_dir}/ to ${optional_plugin_dir}/"
            mv "${main_plugin_dir}/${plugin}" "${optional_plugin_dir}/"
        else
            echo "Moving lib${plugin}.so from ${main_plugin_dir}/ to ${optional_plugin_dir}/"
            mv "${main_plugin_dir}/lib${plugin}.so" "${optional_plugin_dir}/"
        fi
    done
}

distrib_createArchive() # archive dir command...
{
    local -r archive="$1"; shift
    local -r dir="$1"; shift

    rm -rf "${archive}" #< Avoid updating an existing archive.
    echo "  Creating ${archive}"
    ( cd "${dir}" && "$@" "${archive}" * ) #< Subshell prevents "cd" from changing the current dir.
    echo "  Done"
}

# [in] DISTRIBUTION_OUTPUT_DIR
# [in] DISTRIBUTION_TAR_GZ
# [in] GZIP_COMPRESSION_LEVEL
# [in] STAGE
# [in] WORK_DIR
# [in] CURRENT_BUILD_DIR
# [in] DISTRIBUTION_ZIP
# [in] DISTRIBUTION_UPDATE_ZIP
distrib_createZipDistributionPackage()
{
    echo ""
    echo "Creating distribution archive"

    echo "  Creating .tar.gz distribution file"
    local -r tar_gz_archive="${DISTRIBUTION_OUTPUT_DIR}/${DISTRIBUTION_TAR_GZ}"
    GZIP=-${GZIP_COMPRESSION_LEVEL} distrib_createArchive "${tar_gz_archive}" "${STAGE}" tar czf

    echo "  Creating .zip distribution file"
    local -r zip_dir="${WORK_DIR}/zip"
    mkdir -p "${zip_dir}"

    mv "${tar_gz_archive}" "${zip_dir}/"
    install -m 644 "${CURRENT_BUILD_DIR}/package.json" "${zip_dir}/"
    install -m 755 "${CURRENT_BUILD_DIR}/install.sh" "${zip_dir}/"

    # Set the compression level to 0 because there is no reason to try to compress .tar.gz file.
    distrib_createArchive "${DISTRIBUTION_OUTPUT_DIR}/${DISTRIBUTION_ZIP}" "${zip_dir}" zip -0

    echo "  Creating update package"
    cp "${DISTRIBUTION_OUTPUT_DIR}/${DISTRIBUTION_ZIP}" \
        "${DISTRIBUTION_OUTPUT_DIR}/${DISTRIBUTION_UPDATE_ZIP}"
}

# TODO: Check, if we really need this function - may be it would be better to use "install" instead
# of cp everywhere so there will be no need to setting permissions in the separate function.
#
# [in] STAGE
# [in] BIN_INSTALL_PATH
# [in] SERVER_SCRIPTS_DIR
distrib_setPermissionsInStageDir()
{
    echo ""
    echo "Setting permissions"

    find "${STAGE}" -type d -print0 | xargs -0 chmod 755
    find "${STAGE}" -type f -print0 | xargs -0 chmod 644
    chmod -R 755 "${STAGE}/${BIN_INSTALL_PATH}"

    if [[ "${SERVER_SCRIPTS_DIR-}" ]]; then
        chmod 755 "${STAGE}/${SERVER_SCRIPTS_DIR}"/*
    fi
}

# [in] STAGE
# [in] BIN_INSTALL_PATH
# [in] LIB_INSTALL_PATH
distrib_stripDistributionIfNeeded()
{
    if [[ "${STRIP_BINARIES}" != "ON" ]]; then
        return
    fi

    echo ""
    echo "Stripping distribution"

    local -a server_executables=("mediaserver" "root-tool" "testcamera")
    distrib_stripDirectory "${STAGE}/${BIN_INSTALL_PATH}" "${server_executables[@]}"

    local -r lib_path="${ALT_LIB_INSTALL_PATH-"${LIB_INSTALL_PATH}"}"
    distrib_stripDirectory "${STAGE}/${lib_path}"
}

# Prepare to build the distribution archives/packages.
#
# The specified work_dir will be deleted on exit (if not prohibited by the command-line args), but
# should be created after calling this function.
#
# Redirect the output to log_file, if the verbose mode is not requested. Stderr is duplicated
# to the actual stderr, and also accumulated in a file and finally appended to the log file.
#
# Parse common command-line args, in particular:
# - Provide help if needed.
# - Suppress output redirection (verbose mode).
# - Do not delete work_dir on exit (useful for debugging).
#
# [in] distrib_EXTRA_HELP
#
distrib_prepareToBuildDistribution() # work_dir log_file "$@"
{
    distrib_WORK_DIR="$1" && shift
    local -r LOG_FILE="$1" && shift

    distrib_parseArgs "$@"

    trap distrib_onExit EXIT

    rm -rf "$distrib_WORK_DIR"

    if [ $distrib_KEEP_WORK_DIR = 1 ]
    then
        local -r WORK_DIR_NOTE="(ATTENTION: will NOT be deleted)"
    else
        local -r WORK_DIR_NOTE="(will be deleted on success)"
    fi
    echo "Creating distribution in $distrib_WORK_DIR $WORK_DIR_NOTE."

    distrib_STDERR_FILE=""
    if [ $distrib_VERBOSE = 0 ]
    then
        distrib_redirectOutputToLog "$LOG_FILE"
    fi
}
