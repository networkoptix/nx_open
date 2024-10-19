#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e

HELP_MESSAGE="Usage: $(basename $0) <Application ID> <Target Directory>"

if [ $# -ne 2 ]
then
    echo "$HELP_MESSAGE" >&2
    exit 1
fi

APP_ID="$1"
TARGET_DIR="$2"

PROFILES_DIR="${HOME}/Library/MobileDevice/Provisioning Profiles"
FIND_PROFILE_SCRIPT="$(dirname $0)/find_profile_for_app_id.sh"

PROFILE_ID=$("${FIND_PROFILE_SCRIPT}" "${APP_ID}")
cp "${PROFILES_DIR}/${PROFILE_ID}.mobileprovision" "${TARGET_DIR}/embedded.mobileprovision"
