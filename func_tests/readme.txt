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

Arguments to store results in JunkShop

-p junk_shop.pytest_plugin \
--capture-db=postgres:XXXXXXXXXXXX@10.0.0.113 \
--build-parameters=project=test,branch=vms,build_num=XXXX,platform=linux-x64,customization=default,cloud_group=test
