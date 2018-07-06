#!/bin/bash
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

CUSTOMIZATION=$1
if [ -z "$CUSTOMIZATION" ]
then
    CUSTOMIZATION="default"
fi

SKIN=$2
if [ -z "$SKIN" ]
then
    SKIN="blue"
fi
./build_skin.sh $SKIN

. ../env/bin/activate
cd ../cloud
python manage.py filldata $CUSTOMIZATION
