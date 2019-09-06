#!/bin/bash
source "$(dirname $0)/utils.sh"

CLOUD_HOST_KEY="this_is_cloud_host_name"
FILE_PATH_REGEX=".*/\(libnx_network\.\(a\|so\)\|nx_network\.dll\)"

# From C++ source:
CLOUD_HOST_NAME_WITH_KEY=$(eval echo \
"this_is_cloud_host_name cloud-test.hdw.mx\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")

#--------------------------------------------------------------------------------------------------

# If not done yet, scan from current dir upwards to find root repository dir (e.g. develop/nx_vms),
# or, if not found, set VMS_DIR to the current dir.
# [in,out] VMS_DIR
find_VMS_DIR()
{
    nx_find_parent_dir VMS_DIR "develop"
}

# [in] FILE
save_backup()
{
    local BAK_FILE="$FILE.patch-cloud-host.BAK"
    cp "$FILE" "$BAK_FILE" || nx_fail "failed: cp $FILE $BAK_FILE"
    nx_echo "Backup saved: $BAK_FILE"
}

# If no CLOUD_HOST_KEY, show the current text, otherwise, patch FILE to set the new text.
# [in] FILE
# [in] CLOUD_HOST_KEY
# [in] NEW_CLOUD_HOST
process_file()
{
    local STRING=$(grep -boPa "${CLOUD_HOST_KEY}[^\x0]*" "$FILE" | tr ':' ' ')
    [ -z "$STRING" ] && nx_fail "'$CLOUD_HOST_KEY' string not found in $FILE"

    local OFFSET=$(echo "$STRING" |awk '{print $1}')
    local EXISTING_CLOUD_HOST=$(echo "$STRING" |awk '{print $3}')

    if [ -z "$NEW_CLOUD_HOST" ]; then
        nx_echo "Cloud Host is '$EXISTING_CLOUD_HOST' in $FILE"
    elif [ "$NEW_CLOUD_HOST" = "$EXISTING_CLOUD_HOST" ]; then
        nx_echo "Cloud Host is already '$EXISTING_CLOUD_HOST' in $FILE"
    else
        save_backup

        local PATCH_OFFSET=$(expr "$OFFSET" + ${#CLOUD_HOST_KEY} + 1)
        local NUL_OFFSET=$(expr "$PATCH_OFFSET" + ${#NEW_CLOUD_HOST})
        local NUL_LEN=$(expr \
            ${#CLOUD_HOST_NAME_WITH_KEY} - ${#CLOUD_HOST_KEY} - 1 - ${#NEW_CLOUD_HOST})
        nx_log "CLOUD_HOST_NAME_WITH_KEY len: ${#CLOUD_HOST_NAME_WITH_KEY}"
        nx_log "CLOUD_HOST_KEY len: ${#CLOUD_HOST_KEY}"

        # Patching the printable chars.
        echo -ne "$NEW_CLOUD_HOST" |dd bs=1 conv=notrunc seek="$PATCH_OFFSET" of="$FILE" \
            2>/dev/null || nx_fail "failed: dd of text"

        # Filling the remaining bytes with NUL chars.
        dd bs=1 conv=notrunc seek="$NUL_OFFSET" of="$FILE" if=/dev/zero count="$NUL_LEN" \
            2>/dev/null || nx_fail "failed: dd of NULs"

        nx_echo "Cloud Host was '$EXISTING_CLOUD_HOST', now is '$NEW_CLOUD_HOST' in $FILE"
    fi
}

#---------------------------------------------------------------------------------------------------

help()
{
    cat <<EOF
Utility to patch a binary file to replace the hard-coded Cloud Host.

Show current value:
    $(basename "$0") [--verbose] --show [path/to/binary_to_patch]

Patch with new value:
    $(basename "$0") [--verbose] 'new_cloud_host' [path/to/binary_to_patch]

Cloud Host resides in: VMS 3.1 - nx_network library (.so, .a, or .dll), VMS 3.0 - each .exe for
Windows, libcommon.so for Linux, and libcommon.a for Android.

If the binary to patch is not specified, a search is performed considering VMS 3.1, and the current
directory should be:
- For Maven build: any directory inside root of nx_vms repo.
- For CMake build, or installed VMS: any directory containing nx_network binary at any level.

EOF
}

main()
{
    local NEW_CLOUD_HOST
    [ "$1" != "--show" ] && NEW_CLOUD_HOST="$1"

    # Determine the file to patch either from argv[2] or by searching.
    local FILE
    if [ $# -ge 2 ]; then
        FILE="$2"
        [ ! -f "$FILE" ] && nx_fail "Specified file does not exist: $FILE"
    else
        local VMS_DIR
        find_VMS_DIR

        nx_find_file FILE "the file to patch" "$VMS_DIR" -regex "$FILE_PATH_REGEX"
    fi

    process_file
}

nx_run "$@"
