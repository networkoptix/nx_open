import os

import pytest

from defaults import defaults
from framework.cloud_host import CloudAccountFactory, resolve_cloud_host_from_registry


def pytest_addoption(parser):
    group = parser.getgroup('cloud')
    group.addoption('--cloud-group', default=defaults['cloud_group'], help=(
        'Cloud group; cloud host for it will be requested from ireg.hdw.mx; '
        'default is %(default)r'))
    group.addoption('--autotest-email-password', help=(
        'Password for accessing service account via IMAP protocol. '
        'Used for activation cloud accounts for different cloud groups and customizations.'))


@pytest.fixture(scope='session')
def customization(mediaserver_installers):
    customizations = set(installer.customization for installer in mediaserver_installers.values())
    customization, = customizations  # This should be checked in mediaserver_installers fixture.
    return customization


@pytest.fixture()
def cloud_group(request):
    return request.config.getoption('--cloud-group')


@pytest.fixture()
def cloud_host(customization, cloud_group):
    return resolve_cloud_host_from_registry(cloud_group, customization.customization_name)


@pytest.fixture()
def cloud_account_factory(request, customization, cloud_host):
    return CloudAccountFactory(
        request.config.getoption('--cloud-group'),
        customization.customization_name,
        cloud_host,
        request.config.getoption('--autotest-email-password') or os.environ.get('AUTOTEST_EMAIL_PASSWORD'))


# CloudAccount instance
@pytest.fixture()
def cloud_account(cloud_account_factory):
    return cloud_account_factory()
