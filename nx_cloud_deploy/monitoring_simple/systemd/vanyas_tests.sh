#!/bin/bash

VERSION=1.9

docker run -e AWS_DEFAULT_REGION=us-east-1 -v /var/run/docker.sock:/var/run/docker.sock --log-driver=awslogs --log-opt awslogs-region=us-east-1 --log-opt awslogs-group=prod-portal_tests --log-opt awslogs-stream=vanyas_tests --rm 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/monitoring_simple:$VERSION nxvms.com
docker run --rm -e AWS_DEFAULT_REGION=us-east-1 governmentpaas/awscli aws cloudwatch put-metric-data --namespace "Simple Test" --metric-name Failures --value $?
