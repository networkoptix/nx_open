import pytest

from framework.server_physical_host import PhysicalInstallationCtl


@pytest.fixture(scope='session')
def physical_installation_ctl(tests_config, ca, mediaserver_deb):
    if not tests_config:
        return None
    pic = PhysicalInstallationCtl(
        mediaserver_deb.path,
        mediaserver_deb.customization.company,
        tests_config.physical_installation_host_list,
        ca)
    return pic
