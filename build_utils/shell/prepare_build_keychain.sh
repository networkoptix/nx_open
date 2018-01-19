#!/bin/bash

set -e

KEYCHAIN="nx_build"
KEYCHAIN_PASSWORD="qweasd123"
CERT=""
CERT_PASSWORD=""

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
    case $1 in
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

KEYCHAINS=$(security list-keychains -d user | sed 's/"//g')

if ! echo "$KEYCHAINS" | grep -q "$KEYCHAIN"
then
    echo "Creating keychain $KEYCHAIN"

    security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN" 2> /dev/null || true
    security list-keychains -d user -s "$KEYCHAIN" $KEYCHAINS
fi

echo "Unlocking keychain $KEYCHAIN"

security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
security set-keychain-settings "$KEYCHAIN"

if [ -n "$CERT" ]
then
    echo "Importing certificates from $CERT"

    security import "$CERT" -k "$KEYCHAIN" -P "$CERT_PASSWORD" -T "/usr/bin/codesign"
fi

security set-key-partition-list -S apple: -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN" \
    > /dev/null 2> /dev/null

