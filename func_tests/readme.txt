This is functional tests for nx witness server.

See
https://networkoptix.atlassian.net/wiki/display/SD/Functional+test+environment
for HOWTO use them

To run these functional tests following requirements must be met:
* python-opencv, virtualbox and vagrant >= 1.8 packages are installed:
  $ sudo apt-get install virtualbox vagrant python-opencv
* python packages from requirements.txt is installed:
  $ virtualenv --system-site-packages venv
  $ . venv/bin/activate
  $ pip install -r requirements.txt

See also devtools/ci/linux-x64/setup-build-box-linux.sh
This script can be used to prepare vagrant box for running functional tests on it.
