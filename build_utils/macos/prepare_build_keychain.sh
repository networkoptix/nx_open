#!/bin/bash

set -e

KEYCHAIN="nx_build"
KEYCHAIN_PASSWORD="qweasd123"
CERT=""
CERT_PASSWORD=""
IGNORE_IMPORT_ERRORS=0

HELP_MESSAGE="Usage: $(basename $0) [options]
Prepare a keychain for codesigning the project.
By default it creates a keychain $KEYCHAIN with password $KEYCHAIN_PASSWORD if it does not exist.

Options:
  -k, --keychain                Keychain name (default $KEYCHAIN)
  -p, --keychain-password       Keychain password (default $KEYCHAIN)
  -c, --certificate             Certificate file to import
  -P, --certificate-password    Certificate file password"

while [[ $# -gt 0 ]]
do
    case "$1" in
        -k|--keychain)
            KEYCHAIN="$2"
            shift
            shift
            ;;
        -p|--keychain-password)
            KEYCHAIN_PASSWORD="$2"
            shift
            shift
            ;;
        -c|--certificate)
            CERT="$2"
            shift
            shift
            ;;
        -P|--certificate-password)
            CERT_PASSWORD="$2"
            shift
            shift
            ;;
        -q|--ignore-import-errors)
            IGNORE_IMPORT_ERRORS=1
            shift
            ;;
        -h|--help)
            echo "$HELP_MESSAGE"
            exit
            ;;
        *)
            echo "Unknown argument $1"
            exit 1
            ;;
    esac
done

[[ $KEYCHAIN == *.keychain ]] && KEYCHAIN="${KEYCHAIN%.keychain}"

KEYCHAIN="$HOME/Library/Keychains/$KEYCHAIN"

KEYCHAINS=$(security list-keychains -d user | sed 's/"//g')

if ! echo "$KEYCHAINS" | grep -q "$KEYCHAIN"
then
    echo "Creating keychain $KEYCHAIN"

    security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN" 2> /dev/null || true
    security list-keychains -d user -s "$KEYCHAIN" $KEYCHAINS
fi

echo "Unlocking keychain $KEYCHAIN"

security default-keychain -s "$KEYCHAIN"
security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
security set-keychain-settings "$KEYCHAIN"

if [ -n "$CERT" ]
then
    echo "Importing certificates from $CERT"

    ERROR_FLAG="-e"
    [ $IGNORE_IMPORT_ERRORS = 1 ] && ERROR_FLAG="+e"
    (set $ERROR_FLAG; \
        security import "$CERT" -k "$KEYCHAIN" -P "$CERT_PASSWORD" -T "/usr/bin/codesign"; \
        true)
fi

security set-key-partition-list -S apple: -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN" \
    > /dev/null 2> /dev/null || true
