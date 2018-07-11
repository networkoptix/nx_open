import logging
import re

from framework.os_access.exceptions import DoesNotExist
from framework.serialize import dump, load

_logger = logging.getLogger(__name__)

_REGEX = re.compile(
    br'(?<=this_is_cloud_host_name )'  # Mind space.
    br'(?P<host>.+?)(?P<nulls>\0+)'
    br'(?=\0)')  # String must end with \0.


def set_cloud_host(installation, new_host):
    # Path to .so with cloud host string. Differs among versions.
    for lib_name in {'lib/libnx_network.so', 'lib/libcommon.so', 'nx_network.dll'}:
        lib_path = installation.dir / lib_name
        info_path = lib_path.with_name(lib_path.name + '.info')
        version = installation.identity.version
        try:
            info_raw = info_path.read_text()
        except DoesNotExist:
            info = {}
            _logger.debug("No info file at %s.", info_path)
        else:
            info = load(info_raw)
            if info['version'] == version.as_str:  # Assume `installation` is what is actually installed.
                try:
                    cloud_host_here = info['cloud_host_here']
                except KeyError:
                    pass
                else:
                    if cloud_host_here:
                        if info['cloud_host'] == new_host:  # Always in if cloud host here.
                            _logger.info("Host in lib %s is same: %s.", lib_path, new_host)
                            continue
                    else:
                        _logger.info("No cloud host string in lib %s.", lib_path)
                        continue
            else:
                info = {}  # Reset info -- it's for another version.

        original_info = info.copy()

        try:
            try:
                offset = info['cloud_host_offset']
                max_length = info['cloud_host_max_length']
                old_host = info['cloud_host']
            except KeyError:
                try:
                    lib_bytes_original = lib_path.read_bytes()
                except DoesNotExist:
                    _logger.info("File %s doesn't exist.", lib_path)
                    continue
                match = _REGEX.search(lib_bytes_original)
                if match is None:
                    _logger.info("No cloud host string in lib %s.", lib_path)
                    info['version'] = version.as_str
                    info['cloud_host_here'] = False
                    continue
                offset = match.start()
                max_length = match.end() - match.start()
                old_host = match.group('host')
                info['version'] = version.as_str
                info['cloud_host_here'] = True
                info['cloud_host'] = old_host  # Will update after actually written.
                info['cloud_host_offset'] = offset
                info['cloud_host_max_length'] = max_length

            if len(new_host) > max_length:
                raise ValueError("Length of new host must be at most {}".format(max_length))

            _logger.info("Replace cloud host in %s: %s with %s.", lib_path, old_host, new_host)
            lib_path.write_bytes(new_host.encode('ascii').ljust(max_length, '\0'), offset=offset)
            info['cloud_host'] = new_host
        finally:
            if info != original_info:
                info_path.write_text(dump(info))
