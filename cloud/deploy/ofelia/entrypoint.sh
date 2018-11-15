#!/bin/sh

mkdir -p /etc/ofelia

config=$(echo "$MODULE_CONFIGURATION" | base64 -d)

# Input can be in 2 forms. JSON encoded and not.
if [ "${config:0:1}" = '"' ]
then
    echo "$config" | jq -r . > /etc/ofelia/config.ini
else
    echo "$config" > /etc/ofelia/config.ini
fi

/usr/local/bin/ofelia daemon --config /etc/ofelia/config.ini
