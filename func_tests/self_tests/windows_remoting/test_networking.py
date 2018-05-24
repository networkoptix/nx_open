import pytest

from framework.networking.windows import WindowsNetworking


@pytest.fixture(scope='session')
def macs(windows_vm_info):
    return windows_vm_info.macs


@pytest.fixture(scope='session')
def networking(macs, winrm):
    return WindowsNetworking(winrm, macs)


def test_interfaces(networking, windows_vm_info):
    assert networking.interfaces
    assert set(windows_vm_info.macs.values()) == set(networking.interfaces.keys())
    assert len(set(networking.interfaces.values())) == len(networking.interfaces) == len(windows_vm_info.macs)
    assert all(networking.interfaces.values())


def test_firewall_rule(networking):
    networking.remove_firewall_rule()
    assert not networking.firewall_rule_exists()
    networking.create_firewall_rule()
    assert networking.firewall_rule_exists()
    networking.create_firewall_rule()
    assert networking.firewall_rule_exists()
    networking.remove_firewall_rule()
    assert not networking.firewall_rule_exists()
    networking.remove_firewall_rule()
    assert not networking.firewall_rule_exists()


def test_ip_addresses(networking, macs):
    networking.remove_ips()
    assert not networking.list_ips()

    macs_iter = iter(macs.values())
    random_mac_1 = next(macs_iter)
    networking.setup_ip(random_mac_1, '10.254.252.2', 28)
    assert len(networking.list_ips()) == 1
    random_mac_2 = next(macs_iter)
    networking.setup_ip(random_mac_2, '10.254.252.18', 28)
    assert len(networking.list_ips()) == 2

    networking.remove_ips()
    assert not networking.list_ips()
    networking.remove_ips()
    assert not networking.list_ips()


def test_routes(networking, macs):
    networking.remove_routes()
    assert not networking.list_routes()

    macs_iter = iter(macs.values())
    random_mac_1 = next(macs_iter)
    networking.route('10.254.252.0/28', random_mac_1, '10.254.251.1')
    assert len(networking.list_routes()) == 1
    random_mac_2 = next(macs_iter)
    networking.route('10.254.252.16/28', random_mac_2, '10.254.251.1')
    assert len(networking.list_routes()) == 2

    networking.remove_routes()
    assert not networking.list_routes()
    networking.remove_routes()
    assert not networking.list_routes()


def test_ping_localhost(networking):
    networking.ping('127.0.0.1')


def test_ping_invalid_address(networking):
    assert not networking.can_reach('127.0.0.0')


def test_internet_access(networking):
    networking.enable_internet()
    assert networking.can_reach('8.8.8.8', timeout_sec=2)
    networking.disable_internet()
    assert not networking.can_reach('8.8.8.8', timeout_sec=4)
