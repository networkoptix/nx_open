import logging
from contextlib import contextmanager

from framework.core_file_traceback import create_core_file_traceback
from framework.dpkg_installation import install_mediaserver
from framework.os_access.path import copy_file
from framework.rest_api import RestApi
from framework.service import UpstartService
from .artifact import ArtifactType
from .mediaserver import Mediaserver

log = logging.getLogger(__name__)

CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')
SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')


def make_dirty_mediaserver(mediaserver_name, machine, mediaserver_deb):
    """Install mediaserver if needed and construct its object. Nothing else."""
    service = UpstartService(machine.os_access, mediaserver_deb.customization.service)
    installation = install_mediaserver(machine.os_access, mediaserver_deb)
    original_port = 7001
    hostname_from_here, port_from_here = machine.ports['tcp', original_port]
    api = RestApi(mediaserver_name, hostname_from_here, port_from_here)
    server = Mediaserver(mediaserver_name, service, installation, api, machine, original_port)
    return server


def cleanup_mediaserver(mediaserver, ca):
    """Stop and remove everything produced by previous runs. Make installation "fresh"."""
    mediaserver.stop(already_stopped_ok=True)
    mediaserver.installation.cleanup_core_files()
    mediaserver.installation.cleanup_var_dir()
    mediaserver.installation.put_key_and_cert(ca.generate_key_and_cert())
    mediaserver.installation.restore_mediaserver_conf()
    mediaserver.installation.update_mediaserver_conf({
        'logLevel': 'DEBUG2',
        'tranLogLevel': 'DEBUG2',
        'checkForUpdateUrl': 'http://127.0.0.1:8080',  # TODO: Use fake server responding with small updates.
        })


def setup_clean_mediaserver(mediaserver_name, machine, mediaserver_deb, ca):
    """Get mediaserver as if it hasn't run before."""
    mediaserver = make_dirty_mediaserver(mediaserver_name, machine, mediaserver_deb)
    cleanup_mediaserver(mediaserver, ca)
    return mediaserver


class MediaserverExaminationError(Exception):
    pass


class MediaserverStopped(MediaserverExaminationError):
    pass


class MediaserverUnresponsive(MediaserverExaminationError):
    pass


def examine_mediaserver(mediaserver, stopped_ok=False):
    examination_logger = log.getChild('examination')
    if mediaserver.service.is_running():
        examination_logger.info("%r is running.", mediaserver)
        if mediaserver.is_online():
            examination_logger.info("%r is online.", mediaserver)
        else:
            mediaserver.service.make_core_dump()
            raise MediaserverUnresponsive('{} is not online; core dump made.'.format(mediaserver))
    else:
        if stopped_ok:
            examination_logger.info("%r is stopped; it's OK.", mediaserver)
        else:
            raise MediaserverStopped("{} is stopped.".format(mediaserver))


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
            mediaserver.machine.os_access,
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
    def __init__(self, artifact_factory, mediaserver_deb, ca, cloud_host):
        self._artifact_factory = artifact_factory
        self._allocated_servers = []
        self._ca = ca
        self._mediaserver_deb = mediaserver_deb
        self._cloud_host = cloud_host

    @contextmanager
    def allocated_mediaserver(self, name, vm):
        mediaserver = setup_clean_mediaserver(name, vm, self._mediaserver_deb, self._ca)
        mediaserver.installation.patch_binary_set_cloud_host(self._cloud_host)  # TODO: Call this in appropriate place.
        yield mediaserver
        examine_mediaserver(mediaserver)
        collect_artifacts_from_mediaserver(mediaserver, self._artifact_factory)
