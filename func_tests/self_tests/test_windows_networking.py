def test_firewall_rule(windows_networking):
    windows_networking.remove_firewall_rule()
    assert not windows_networking.firewall_rule_exists()
    windows_networking.create_firewall_rule()
    assert windows_networking.firewall_rule_exists()
    windows_networking.create_firewall_rule()
    assert windows_networking.firewall_rule_exists()
    windows_networking.remove_firewall_rule()
    assert not windows_networking.firewall_rule_exists()
    windows_networking.remove_firewall_rule()
    assert not windows_networking.firewall_rule_exists()


def test_ip_addresses(windows_networking, windows_macs):
    windows_networking.remove_ips()
    assert not windows_networking.list_ips()

    macs_iter = iter(windows_macs.values())
    random_mac_1 = next(macs_iter)
    windows_networking.setup_ip(random_mac_1, '10.254.252.2', 28)
    assert len(windows_networking.list_ips()) == 1
    random_mac_2 = next(macs_iter)
    windows_networking.setup_ip(random_mac_2, '10.254.252.18', 28)
    assert len(windows_networking.list_ips()) == 2

    windows_networking.remove_ips()
    assert not windows_networking.list_ips()
    windows_networking.remove_ips()
    assert not windows_networking.list_ips()


def test_routes(windows_networking, windows_macs):
    windows_networking.remove_routes()
    assert not windows_networking.list_routes()

    macs_iter = iter(windows_macs.values())
    random_mac_1 = next(macs_iter)
    windows_networking.route('10.254.252.0/28', random_mac_1, '10.254.251.1')
    assert len(windows_networking.list_routes()) == 1
    random_mac_2 = next(macs_iter)
    windows_networking.route('10.254.252.16/28', random_mac_2, '10.254.251.1')
    assert len(windows_networking.list_routes()) == 2

    windows_networking.remove_routes()
    assert not windows_networking.list_routes()
    windows_networking.remove_routes()
    assert not windows_networking.list_routes()
