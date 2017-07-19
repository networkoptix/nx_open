#!/bin/bash


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

docker run --rm -i 009544449203.dkr.ecr.us-east-1.amazonaws.com/devtools/wheel_uploader:1.0-py2 < $DIR/../../cloud_portal/cloud/requirements.txt
