#!/bin/bash -xe

export PGPASSWORD="$1"
if [[ "$PGPASSWORD" == "" ]]; then
    echo "Usage: $0 <pg-password>"
    exit 1
fi

shift

export PYTHONPATH=$HOME/Development/devtools/ci/junk_shop
export PYTEST_PLUGINS=junk_shop.pytest_plugin

POSTGRES_CREDENTIALS=postgres:${PGPASSWORD}@megatron
CUSTOMIZATION=hanwha
BUILD_PARAMETERS=project=test,branch=vms_3.2_dev,build_num=998,platform=linux-x64,customization=${CUSTOMIZATION},cloud_group=test

. $HOME/Development/func_tests_venv/bin/activate

cd $HOME/Development/nx_vms/func_tests

pytest -vv \
    --capture-db=${POSTGRES_CREDENTIALS} \
    --work-dir=$HOME/run/funtest \
    --bin-dir=$HOME/run/bin \
    --vm-address=10.0.3.91 \
    --build-parameters=${BUILD_PARAMETERS} \
    --customization=${CUSTOMIZATION} \
    --instafail \
    "$@"

# cd ~/Development/nx_vms/func_tests && ~/venv/bin/py.test -vs --nocapturelog --work-dir=$HOME/run/funtest --bin-dir=$HOME/run/funtest-wd $@
# cd ~/run/funtest-wd/ && ~/venv/bin/py.test -vs ~/Development/nx_vms/func_tests/ --work-dir=$HOME/run/funtest