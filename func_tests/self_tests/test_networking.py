def test_interfaces(networking, macs):
    assert networking.interfaces
    assert set(macs.values()) == set(networking.interfaces.keys())
    assert len(set(networking.interfaces.values())) == len(networking.interfaces) == len(macs)
    assert all(networking.interfaces.values())


def test_ping_localhost(networking):
    assert networking.can_reach('127.0.0.1')


def test_ping_invalid_address(networking):
    assert not networking.can_reach('192.0.2.1')


def test_internet_access(networking):
    networking.enable_internet()
    assert networking.can_reach('8.8.8.8', timeout_sec=2)
    networking.disable_internet()
    assert not networking.can_reach('8.8.8.8', timeout_sec=4)
