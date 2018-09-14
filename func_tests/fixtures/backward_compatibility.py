def pytest_addoption(parser):
    group = parser.getgroup("Backward compatibility", description='backward compatibility with func_tests from vms_3.2')
    group.addoption('--vm-port-base', help="Specified in configuration. Ports are calculated differently now.")
    group.addoption('--vm-name-prefix', help="Backward compatibility. Ignored.")
    group.addoption('--vm-host', help="Tests now can run only locally.")
    group.addoption('--vm-host-user', help="Specified in VM description.")
    group.addoption('--vm-host-key', help="Specified in VM description.")
    group.addoption('--vm-host-dir', help="Specified in VM description.")
