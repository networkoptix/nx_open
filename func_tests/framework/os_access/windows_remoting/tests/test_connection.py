import logging

from framework.os_access.windows_remoting.cmd import Shell

log = logging.getLogger(__name__)


def test_shell(protocol):
    with Shell(protocol) as shell:
        yield shell
