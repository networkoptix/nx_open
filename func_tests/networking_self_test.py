from pprint import pprint

import pytest
import yaml
from pathlib2 import Path

from test_utils.networking import setup_networks

SAMPLES_DIR = Path(__file__).parent / 'test_utils' / 'networking'


@pytest.mark.parametrize('sample', ['nested_nat'])
def test_setup(vm_factory, sample):
    vms = [vm_factory() for _ in range(5)]
    sample_file = SAMPLES_DIR / ('sample_' + sample + '.net.yaml')
    sample = yaml.load(sample_file.read_text())
    nodes = {vm.networking for vm in vms}
    assigned_nodes = setup_networks(nodes, sample['networks'], default_gateways=sample['default_gateways'] or dict())
    pprint(assigned_nodes)
