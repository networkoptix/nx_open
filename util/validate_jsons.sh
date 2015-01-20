#!/bin/bash

JSONLINT=`which jsonlint`

if [ -z "$JSONLINT" ];	then
    echo 'invalid JSONLINT - trying to install...'
	sudo apt-get install -y python-demjson
fi

JSONLINT=`which jsonlint`
if [ -z "$JSONLINT" ];	then
    echo 'could not install JSONLINT - compilation terminated'
	exit 1
fi