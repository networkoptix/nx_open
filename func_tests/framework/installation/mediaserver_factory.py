import logging

from framework.artifact import ArtifactType
from framework.installation.make_installation import make_installation
from framework.installation.mediaserver import Mediaserver
from framework.os_access.path import copy_file

_logger = logging.getLogger(__name__)

CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')
SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')


class NoSupportedInstaller(Exception):
    def __init__(self, installation, installers):
        super(NoSupportedInstaller, self).__init__(
            "{!r} supports none of {!r}".format(installation, installers))


def setup_mediaserver(os_access, installers, ssl_key_cert):
    """Get mediaserver as if it hasn't run before."""
    customization, = {installer.customization for installer in installers}
    installation = make_installation(os_access, customization)
    supported_installers = list(filter(installation.can_install, installers))
    if not supported_installers:
        raise NoSupportedInstaller(installation, installers)
    installer, = supported_installers
    installation.install(installer)
    mediaserver = Mediaserver(os_access.alias, installation)
    mediaserver.stop(already_stopped_ok=True)
    mediaserver.installation.cleanup(ssl_key_cert)
    return mediaserver


def examine_mediaserver(mediaserver, stopped_ok=False):
    examination_logger = _logger.getChild('examination')
    status = mediaserver.service.status()
    if status.is_running:
        examination_logger.info("%r is running.", mediaserver)
        if mediaserver.api.is_online():
            examination_logger.info("%r is online.", mediaserver)
        else:
            mediaserver.os_access.make_core_dump(status.pid)
            _logger.error('{} is not online; core dump made.'.format(mediaserver))
    else:
        if stopped_ok:
            examination_logger.info("%r is stopped; it's OK.", mediaserver)
        else:
            _logger.error("{} is stopped.".format(mediaserver))


def collect_mediaserver_artifacts(mediaserver, destination_dir):
    for file in mediaserver.installation.list_log_files():
        copy_file(file, destination_dir / file.name)
    for core_dump in mediaserver.installation.list_core_dumps():
        local_core_dump_path = destination_dir / core_dump.name
        copy_file(core_dump, local_core_dump_path)
        # noinspection PyBroadException
        try:
            traceback = mediaserver.installation.parse_core_dump(core_dump)
            backtrace_name = local_core_dump_path.name + '.backtrace.txt'
            local_traceback_path = local_core_dump_path.with_name(backtrace_name)
            local_traceback_path.write_text(traceback)
        except Exception:
            _logger.exception("Cannot parse core dump: %s.", core_dump)
