#!/bin/sh

eval echo -e $(echo "$MODULE_CONFIGURATION" | base64 -d) > /etc/ofelia/config.ini

/usr/local/bin/ofelia daemon --config /etc/ofelia/config.ini
