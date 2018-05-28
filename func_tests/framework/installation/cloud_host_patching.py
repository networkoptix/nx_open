import logging
import re

from framework.os_access.exceptions import DoesNotExist

_logger = logging.getLogger(__name__)


def set_cloud_host(installation, new_host):
    regex = re.compile(
        br'(?<=this_is_cloud_host_name )'  # Mind space.
        br'[^\0]+\0+'
        br'(?=\0)')  # String must end with \0.

    # Path to .so with cloud host string. Differs among versions.
    for lib_name in {'lib/libnx_network.so', 'lib/libcommon.so', 'nx_network.dll'}:
        lib_path = installation.dir / lib_name
        try:
            lib_bytes_original = lib_path.read_bytes()
        except DoesNotExist:
            _logger.warning("Lib %s doesn't exist.", lib_path)
            continue

        lib_bytes = bytearray(lib_bytes_original)
        match = regex.search(lib_bytes_original)
        if match is None:
            _logger.warning("No cloud host string in lib %s.", lib_path)
            continue

        old_host = match.group().rstrip(b'\0')
        if new_host == old_host:
            _logger.info("Hosts in lib %s are same: %s.", lib_path, old_host)
        else:
            _logger.info("Replace cloud host in %s: %s with %s.", lib_path, old_host, new_host)
            match_length = match.end() - match.start()
            assert len(new_host) <= match_length, "Length of new host must be at most {}".format(match_length)
            lib_bytes[match.start():match.end()] = new_host.ljust(match_length, b'\0')
            assert len(lib_bytes) == len(lib_bytes_original)
            lib_path.write_bytes(lib_bytes)
