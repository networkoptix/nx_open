This is functional tests for Network Optix Media Server and its customizations.

See
https://networkoptix.atlassian.net/wiki/display/SD/Functional+test+environment
for HOWTO use them

Run `bootstrap.sh` to prepare environment.

To run these functional tests following requirements must be met:
* python-opencv and virtualbox packages are installed:
  $ sudo apt-get install virtualbox python-opencv
* python packages from requirements.txt is installed:
  $ virtualenv --system-site-packages venv
  $ . venv/bin/activate
  $ pip install -r requirements.txt
