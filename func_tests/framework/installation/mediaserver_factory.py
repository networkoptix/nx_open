import logging
from contextlib import contextmanager

from framework.artifact import ArtifactType
from framework.installation.make_installation import installer_by_vm_type, make_installation
from framework.installation.mediaserver import Mediaserver
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.path import copy_file

_logger = logging.getLogger(__name__)

CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')
SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')


def make_dirty_mediaserver(name, installation, installer):
    """Install mediaserver if needed and construct its object. Nothing else."""
    installation.install(installer)
    mediaserver = Mediaserver(name, installation)
    return mediaserver


def setup_clean_mediaserver(mediaserver_name, installation, installer, ca):
    """Get mediaserver as if it hasn't run before."""
    mediaserver = make_dirty_mediaserver(mediaserver_name, installation, installer)
    mediaserver.stop(already_stopped_ok=True)
    mediaserver.installation.cleanup(ca.generate_key_and_cert())
    return mediaserver


def examine_mediaserver(mediaserver, stopped_ok=False):
    examination_logger = _logger.getChild('examination')
    status = mediaserver.service.status()
    if status.is_running:
        examination_logger.info("%r is running.", mediaserver)
        if mediaserver.is_online():
            examination_logger.info("%r is online.", mediaserver)
        else:
            mediaserver.os_access.make_core_dump(status.pid)
            _logger.error('{} is not online; core dump made.'.format(mediaserver))
    else:
        if stopped_ok:
            examination_logger.info("%r is stopped; it's OK.", mediaserver)
        else:
            _logger.error("{} is stopped.".format(mediaserver))


def collect_core_dumps_from_mediaserver(mediaserver, artifacts_dir):
    for core_dump in mediaserver.installation.list_core_dumps():
        local_core_dump_path = artifacts_dir / core_dump.name
        copy_file(core_dump, local_core_dump_path)
        # noinspection PyBroadException
        try:
            traceback = mediaserver.installation.parse_core_dump(core_dump)
            backtrace_name = local_core_dump_path.name + '.backtrace.txt'
            local_traceback_path = local_core_dump_path.with_name(backtrace_name)
            local_traceback_path.write_text(traceback)
        except Exception:
            _logger.exception("Cannot parse core dump: %s.", core_dump)


def collect_artifacts_from_mediaserver(mediaserver, artifacts_dir):
    for file in mediaserver.installation.list_log_files():
        copy_file(file, artifacts_dir / file.name)
    collect_core_dumps_from_mediaserver(mediaserver, artifacts_dir)


class MediaserverFactory(object):
    def __init__(self, mediaserver_installers, artifacts_dir, ca):
        self._mediaserver_installers = mediaserver_installers
        self._artifacts_dir = artifacts_dir
        self._ca = ca

    @contextmanager
    def allocated_mediaserver(self, name, vm):
        # While it's tempting to take vm.alias as name and, moreover, it might be a good decision,
        # existing client code structure (ClosingPool class) requires this function to have name parameter.
        # It's wrong but requires human-hours of thinking.
        # TODO: Refactor client code so this method doesn't rely on it.
        installer = installer_by_vm_type(self._mediaserver_installers, vm.type)
        installation = make_installation(self._mediaserver_installers, vm.type, vm.os_access)
        mediaserver = setup_clean_mediaserver(name, installation, installer, self._ca)
        yield mediaserver
        examine_mediaserver(mediaserver)
        mediaserver_artifacts_dir = self._artifacts_dir / mediaserver.name
        mediaserver_artifacts_dir.ensure_empty_dir()
        collect_artifacts_from_mediaserver(mediaserver, mediaserver_artifacts_dir)
