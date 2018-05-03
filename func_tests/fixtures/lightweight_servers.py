import pytest

from framework.lightweight_servers_factory import LWS_BINARY_NAME, LightweightServersFactory


@pytest.fixture()
def lightweight_servers_factory(bin_dir, ca, artifact_factory, physical_installation_ctl):
    test_binary_path = bin_dir / LWS_BINARY_NAME
    assert test_binary_path.exists(), 'Test binary for lightweight servers is missing at %s' % test_binary_path
    lightweight_factory = LightweightServersFactory(artifact_factory, physical_installation_ctl, test_binary_path, ca)
    yield lightweight_factory
    lightweight_factory.release()
