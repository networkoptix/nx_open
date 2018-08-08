#!/bin/sh

mkdir -p /etc/ofelia

echo "$MODULE_CONFIGURATION" | base64 -d | jq -r . > /etc/ofelia/config.ini

/usr/local/bin/ofelia daemon --config /etc/ofelia/config.ini
