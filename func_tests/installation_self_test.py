import logging
import shutil
from subprocess import check_call, check_output

import pytest
from pathlib2 import PurePath

from test_utils.host import RemoteSshHost
from test_utils.server_installation import install_media_server
from test_utils.service import is_running
from test_utils.utils import wait_until

_logger = logging.getLogger(__name__)


@pytest.mark.usefixtures('init_logging')
def test_install(work_dir, bin_dir):
    test_dir = work_dir / PurePath(__file__).stem
    test_dir.mkdir(exist_ok=True)
    ssh_config_path = test_dir / 'ssh.conf'
    vagrant_dir = test_dir / 'vagrant'
    completed_flag_path = vagrant_dir / '.completed'
    if not completed_flag_path.exists():
        shutil.rmtree(str(vagrant_dir), ignore_errors=True)
        vagrant_dir.mkdir(exist_ok=True)
        check_call(['vagrant', 'init', 'ubuntu/trusty64'], cwd=str(vagrant_dir))
        check_call(['vagrant', 'up'], cwd=str(vagrant_dir))
        ssh_config = check_output(['vagrant', 'ssh-config'], cwd=str(vagrant_dir))
        ssh_config_path.write_bytes(ssh_config)
        completed_flag_path.touch()
        host = RemoteSshHost('default', ssh_config_path=ssh_config_path)
        host.run_command(['sudo', 'apt-get', 'update'])
        check_call(['vagrant', 'snapshot', 'save', 'initial'], cwd=str(vagrant_dir))
    else:
        check_call(['vagrant', 'snapshot', 'restore', 'initial'], cwd=str(vagrant_dir))
        host = RemoteSshHost('default', ssh_config_path=ssh_config_path)

    install_media_server(host, 'hanwha', bin_dir / 'mediaserver.deb')

    assert wait_until(lambda: is_running(host, 'hanwha-mediaserver'))
