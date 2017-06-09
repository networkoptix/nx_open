#!/bin/bash

# Steps to set up build environment on ubuntu 14.04
sudo apt-get install -y nodejs-legacy npm
sudo npm install -g bower grunt-cli
pushd front_end
npm install
bower install
popd

virtualenv env
. env/bin/activate
pip install --no-index -r cloud/requirements.txt -f cloud/wheelhouse
