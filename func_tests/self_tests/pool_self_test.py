import pytest
import yaml
from pathlib2 import Path

from test_utils.os_access import LocalAccess
from test_utils.ssh.access_manager import SSHAccessManager
from test_utils.ssh.config import SSHConfig
from test_utils.virtual_box import VirtualBox
from test_utils.vm import Pool, Registry, VMConfiguration


@pytest.fixture()
def pool():
    configuration = yaml.load(Path(__file__).with_name('configuration.yaml').read_text())
    ssh_config = SSHConfig(Path(configuration['ssh']['config']).expanduser())
    ssh_config.reset()
    host_os_access = LocalAccess()
    registry = Registry(host_os_access, host_os_access.expand_path(configuration['vm_host']['registry']), 100)
    access_manager = SSHAccessManager(ssh_config, 'root', Path(configuration['ssh']['key']).expanduser())
    hypervisor = VirtualBox(host_os_access, configuration['vm_host']['address'])
    vm_configuration = VMConfiguration(configuration['vm_types']['linux'])
    pool = Pool(vm_configuration, registry, hypervisor, access_manager)
    yield pool
    pool.close()


def test_get(pool):
    machine = pool.get('oi')
    assert machine.alias == 'oi'
