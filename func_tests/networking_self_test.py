import pytest
import yaml
from pathlib2 import Path

from test_utils.networking import setup_networks

SAMPLES_DIR = Path(__file__).parent / 'test_utils' / 'networking'


@pytest.mark.parametrize('sample', ['nested_nat'])
def test_setup(vm_factory, sample):
    sample_file = SAMPLES_DIR / ('sample_' + sample + '.net.yaml')
    sample = yaml.load(sample_file.read_text())
    setup_networks(vm_factory, sample['networks'], default_gateways=sample.get('default_gateways'))
