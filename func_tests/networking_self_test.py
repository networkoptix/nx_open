from pprint import pprint

import pytest
import yaml
from pathlib2 import Path

from test_utils.networking import setup_networks
from test_utils.networking.ip_on_linux import LinuxIpNode

SAMPLES_DIR = Path(__file__).parent / 'test_utils' / 'networking'


@pytest.mark.parametrize('sample', ['nested_nat'])
def test_setup(vm_factory, sample):
    machines = [vm_factory('updates') for _ in range(5)]
    sample_file = SAMPLES_DIR / ('sample_' + sample + '.net.yaml')
    sample = yaml.load(sample_file.read_text())
    ip_nodes = {machine.virtualbox_name: LinuxIpNode(machine.guest_os_access) for machine in machines}
    assigned_nodes = setup_networks(ip_nodes, sample['networks'], default_gateways=sample['default_gateways'] or dict())
    pprint(assigned_nodes)
