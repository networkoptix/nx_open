import pytest


@pytest.mark.skip('Windows installation not implemented')
def test_api_ping(windows_vm):
    windows_vm.api.get('api/ping')
