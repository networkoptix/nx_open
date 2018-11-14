#!/bin/bash -e

. ../environment
. ../common.sh


function stage()
{
    [ -d env ] || virtualenv -p python3.6 env
    . env/bin/activate

    pip install boto3

    rm -rf stage dist
    mkdir stage dist

    cp -R log_alarm_dispatcher.py templates stage

    cd stage
    
    pip install -t . -r ../requirements.txt

    zip -r ../dist/log_alarm_dispatcher.zip *
}

function deploy()
{
    aws lambda update-function-code --function-name log_alarm_dispatcher --zip fileb://$PWD/../dist/log_alarm_dispatcher.zip
}

main $@
