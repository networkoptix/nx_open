import logging
from contextlib import contextmanager

from framework.api_shortcuts import factory_reset
from framework.artifact import ArtifactType
from framework.core_file_traceback import create_core_file_traceback
from framework.installation.deb_installation import DebInstallation
from framework.installation.make_installation import make_installation
from framework.installation.mediaserver import Mediaserver
from framework.installation.mediaserver_deb import MediaserverDeb
from framework.installation.windows_installation import WindowsInstallation
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.path import copy_file
from framework.waiting import wait_for_true

log = logging.getLogger(__name__)

CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')
SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')


def make_dirty_mediaserver(name, installation):
    """Install mediaserver if needed and construct its object. Nothing else."""
    installation.install()
    mediaserver = Mediaserver(name, installation)
    return mediaserver


def cleanup_mediaserver(mediaserver, ca):
    """Stop and remove everything produced by previous runs. Make installation "fresh"."""
    mediaserver.installation.restore_mediaserver_conf()
    mediaserver.installation.update_mediaserver_conf({
        'logLevel': 'DEBUG2',
        'tranLogLevel': 'DEBUG2',
        'checkForUpdateUrl': 'http://127.0.0.1:8080',  # TODO: Use fake server responding with small updates.
        })
    mediaserver.start(already_started_ok=True)
    for core_dump_path in mediaserver.installation.list_core_dumps():
        core_dump_path.unlink()
    factory_reset(mediaserver.api)
    mediaserver.installation.key_pair.parent.mkdir(parents=True, exist_ok=True)
    mediaserver.installation.key_pair.write_text(ca.generate_key_and_cert())


def setup_clean_mediaserver(mediaserver_name, installation, ca):
    """Get mediaserver as if it hasn't run before."""
    mediaserver = make_dirty_mediaserver(mediaserver_name, installation)
    cleanup_mediaserver(mediaserver, ca)
    return mediaserver


def examine_mediaserver(mediaserver, stopped_ok=False):
    examination_logger = log.getChild('examination')
    if mediaserver.service.is_running():
        examination_logger.info("%r is running.", mediaserver)
        if mediaserver.is_online():
            examination_logger.info("%r is online.", mediaserver)
        else:
            mediaserver.service.make_core_dump()
            log.error('{} is not online; core dump made.'.format(mediaserver))
    else:
        if stopped_ok:
            examination_logger.info("%r is stopped; it's OK.", mediaserver)
        else:
            log.error("{} is stopped.".format(mediaserver))


def collect_logs_from_mediaserver(mediaserver, root_artifact_factory):
    log_contents = mediaserver.installation.read_log()
    if log_contents is not None:
        mediaserver_logs_artifact_factory = root_artifact_factory(
            ['server', mediaserver.name], name='server-%s' % mediaserver.name, artifact_type=SERVER_LOG_ARTIFACT_TYPE)
        log_path = mediaserver_logs_artifact_factory.produce_file_path().write_bytes(log_contents)
        log.debug('log file for server %s, %s is stored to %s', mediaserver.name, mediaserver, log_path)


def collect_core_dumps_from_mediaserver(mediaserver, root_artifact_factory):
    for core_dump in mediaserver.installation.list_core_dumps():
        code_dumps_artifact_factory = root_artifact_factory(
            ['server', mediaserver.name.lower(), core_dump.name],
            name='%s-%s' % (mediaserver.name.lower(), core_dump.name),
            is_error=True,
            artifact_type=CORE_FILE_ARTIFACT_TYPE)
        local_core_dump_path = code_dumps_artifact_factory.produce_file_path()
        copy_file(core_dump, local_core_dump_path)
        traceback = create_core_file_traceback(
            mediaserver.os_access,
            mediaserver.installation.binary,
            mediaserver.installation.dir / 'lib',
            core_dump)
        code_dumps_artifact_factory = root_artifact_factory(
            ['server', mediaserver.name.lower(), core_dump.name, 'traceback'],
            name='%s-%s-tb' % (mediaserver.name.lower(), core_dump.name),
            is_error=True,
            artifact_type=TRACEBACK_ARTIFACT_TYPE)
        local_traceback_path = code_dumps_artifact_factory.produce_file_path()
        local_traceback_path.write_bytes(traceback)
        log.warning(
            'Core dump on %r: %s, %s.',
            mediaserver.name, mediaserver, local_core_dump_path, local_traceback_path)


def collect_artifacts_from_mediaserver(mediaserver, root_artifact_factory):
    collect_logs_from_mediaserver(mediaserver, root_artifact_factory)
    collect_core_dumps_from_mediaserver(mediaserver, root_artifact_factory)


class MediaserverFactory(object):
    def __init__(self, mediaserver_packages, artifact_factory, ca):
        self._mediaserver_packages = mediaserver_packages
        self._artifact_factory = artifact_factory
        self._ca = ca

    @contextmanager
    def allocated_mediaserver(self, vm):
        installation = make_installation(self._mediaserver_packages, vm.type, vm.os_access)
        mediaserver = setup_clean_mediaserver(vm.alias, installation, self._ca)
        yield mediaserver
        examine_mediaserver(mediaserver)
        collect_artifacts_from_mediaserver(mediaserver, self._artifact_factory)
