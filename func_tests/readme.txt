To run these functional tests following requirements must be met:
* python-opencv package is installed:
  $ sudo apt-get install python-opencv
* python packages from requirements.txt is installed:
  $ virtualenv --system-site-packages venv
  $ . venv/bin/activate
  $ pip install -r requirements.txt

See also devtools/ci/linux-x64/setup-build-box-linux.sh
