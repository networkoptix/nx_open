#!/bin/sh

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

echo `grep UUID -A1 -a $1 | grep -o '[-A-Za-z0-9]\{36\}'`
