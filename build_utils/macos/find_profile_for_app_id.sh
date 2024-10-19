#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e

HELP_MESSAGE="Usage: $(basename $0) [options] <Application ID>
Find an identity name for code signing.

Options:
    --print-all         Print all profiles if more than one are found."

UNIQ=1
APP_ID=

PROFILES_DIR="${HOME}/Library/MobileDevice/Provisioning Profiles"

while [[ $# -gt 0 ]]
do
    case "$1" in
        --print-all)
            UNIQ=
            shift
            ;;
        -h|--help)
            echo "${HELP_MESSAGE}"
            exit
            ;;
        *)
            APP_ID="$1"
            shift
            ;;
    esac
done

get_profile_name()
{
    local -r PROFILE="$1"
    security cms -D -i "${PROFILE}" | plutil -extract Name raw -
}

pushd "${PROFILES_DIR}" >/dev/null
FOUND_PROFILES=()
for PROFILE in *
do
    PROFILE_NAME=$(get_profile_name "${PROFILE}")
    if [ "${PROFILE_NAME}" == "iOS Team Provisioning Profile: ${APP_ID}" ]
    then
        IFS=$'\n' FOUND_PROFILES+=("${PROFILE%.mobileprovision}")
    fi
done
popd >/dev/null

[ -z "${FOUND_PROFILES}" ] && exit 1

if [ ${#FOUND_PROFILES[@]} -eq 1 ]
then
    echo "${FOUND_PROFILES}"
    exit
fi

if [ ! -z "${UNIQ}" ]
then
    echo "There are more than one profile found." \
        "Delete obsolete profiles in \"${PROFILES_DIR}\" and try again." >&2
    echo "Found profiles:" >&2
    printf "%s\n" ""${FOUND_PROFILES[@]} >&2
    exit 1
fi

printf "%s\n" ""${FOUND_PROFILES[@]}
