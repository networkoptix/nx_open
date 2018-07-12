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


def cleanup_mediaserver(mediaserver, ca):
    """Stop and remove everything produced by previous runs. Make installation "fresh"."""
    mediaserver.stop(already_stopped_ok=True)
    _logger.info("Remove old core dumps.")
    for core_dump_path in mediaserver.installation.list_core_dumps():
        core_dump_path.unlink()
    try:
        _logger.info("Remove var directory %s.", mediaserver.installation.var)
        mediaserver.installation.var.rmtree()
    except DoesNotExist:
        pass
    _logger.info("Put key pair to %s.", mediaserver.installation.key_pair)
    mediaserver.installation.key_pair.parent.mkdir(parents=True, exist_ok=True)
    mediaserver.installation.key_pair.write_text(ca.generate_key_and_cert())
    _logger.info("Update conf file.")
    mediaserver.installation.restore_mediaserver_conf()
    mediaserver.installation.update_mediaserver_conf({
        'logLevel': 'DEBUG2',
        'tranLogLevel': 'DEBUG2',
        'checkForUpdateUrl': 'http://127.0.0.1:8080',  # TODO: Use fake server responding with small updates.
        })


def setup_clean_mediaserver(mediaserver_name, installation, installer, ca):
    """Get mediaserver as if it hasn't run before."""
    mediaserver = make_dirty_mediaserver(mediaserver_name, installation, installer)
    cleanup_mediaserver(mediaserver, ca)
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


def collect_logs_from_mediaserver(mediaserver, root_artifact_factory):
    log_contents = mediaserver.installation.read_log()
    if log_contents is not None:
        mediaserver_logs_artifact_factory = root_artifact_factory(
            ['server', mediaserver.name], name='server-%s' % mediaserver.name, artifact_type=SERVER_LOG_ARTIFACT_TYPE)
        log_path = mediaserver_logs_artifact_factory.produce_file_path().write_bytes(log_contents)
        _logger.debug('log file for server %s, %s is stored to %s', mediaserver.name, mediaserver, log_path)


def collect_core_dumps_from_mediaserver(mediaserver, root_artifact_factory):
    for core_dump in mediaserver.installation.list_core_dumps():
        code_dumps_artifact_factory = root_artifact_factory(
            ['server', mediaserver.name.lower(), core_dump.name],
            name='%s-%s' % (mediaserver.name.lower(), core_dump.name),
            is_error=True,
            artifact_type=CORE_FILE_ARTIFACT_TYPE)
        local_core_dump_path = code_dumps_artifact_factory.produce_file_path()
        copy_file(core_dump, local_core_dump_path)
        # noinspection PyBroadException
        try:
            traceback = mediaserver.installation.parse_core_dump(core_dump)
            code_dumps_artifact_factory = root_artifact_factory(
                ['server', mediaserver.name.lower(), core_dump.name, 'traceback'],
                name='%s-%s-tb' % (mediaserver.name.lower(), core_dump.name),
                is_error=True,
                artifact_type=TRACEBACK_ARTIFACT_TYPE)
            local_traceback_path = code_dumps_artifact_factory.produce_file_path()
            local_traceback_path.write_text(traceback)
            _logger.warning(
                'Core dump on %r: %s, %s.',
                mediaserver, local_core_dump_path, local_traceback_path)
        except Exception:
            _logger.exception("Cannot parse core dump: %s.", core_dump)


def collect_artifacts_from_mediaserver(mediaserver, root_artifact_factory):
    collect_logs_from_mediaserver(mediaserver, root_artifact_factory)
    collect_core_dumps_from_mediaserver(mediaserver, root_artifact_factory)


class MediaserverFactory(object):
    def __init__(self, mediaserver_installers, artifact_factory, ca):
        self._mediaserver_installers = mediaserver_installers
        self._artifact_factory = artifact_factory
        self._ca = ca

    @contextmanager
    def allocated_mediaserver(self, name, vm):
        # While it's tempting to take vm.alias as name and, moreover, it might be a good decision,
        # existing client code structure (ClosingPool class) requires this function to have name parameter.
        # It's wrong but requires human-hours of thinking.
        # TODO: Refactor client code so this method doesn't rely on it.
        installer = installer_by_vm_type(self._mediaserver_installers, vm.type)
        installation = make_installation(self._mediaserver_installers, vm.type, vm.os_access)
        _logger.info("Mediaserver name %s is not same as VM alias %s. Not a mistake?", name, vm.alias)
        mediaserver = setup_clean_mediaserver(name, installation, installer, self._ca)
        yield mediaserver
        examine_mediaserver(mediaserver)
        collect_artifacts_from_mediaserver(mediaserver, self._artifact_factory)
