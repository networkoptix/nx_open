#!/bin/bash
ARGS=(
-s
-p junk_shop.pytest_plugin
--work-dir=~/Development/func_tests_dir/
--bin-dir=~/Development/func_tests_dir/bin/
--mediaserver-dist-path=~/Downloads/wave-server-4.0.0.569-linux64-beta-test.deb
--reinstall
--cloud-group=test
--customization=hanwha
--timeout=600
--vm-port-base=IGNORED
--vm-name-prefix=IGNORED
--log-level=DEBUG
--max-log-width=10000
--capture-db=postgres:$DBPASSWORD@megatron
--build-parameters=project=test,branch=vms_dev,build_num=569,platform=linux-x64,customization=hanwha,cloud_group=test
)
PYTHONPATH=/home/george/Development/devtools/ci/junk_shop PYTEST_PLUGINS=junk_shop.pytest_plugin pytest "${ARGS[@]}" "$@"
