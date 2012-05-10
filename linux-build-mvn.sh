#!/bin/bash

. ~/.profile
sudo chown -R `whoami`:sudo ./
#mvn clean package