import logging

from test_utils.core_file_traceback import create_core_file_traceback
from test_utils.rest_api import RestApi
from test_utils.server_installation import install_mediaserver
from test_utils.service import UpstartService
from .artifact import ArtifactType
from .server import Server

log = logging.getLogger(__name__)

CORE_FILE_ARTIFACT_TYPE = ArtifactType(name='core')
TRACEBACK_ARTIFACT_TYPE = ArtifactType(name='core-traceback')
SERVER_LOG_ARTIFACT_TYPE = ArtifactType(name='log', ext='.log')


class ServerFactory(object):
    def __init__(self, artifact_factory, machines, mediaserver_deb, ca):
        self._artifact_factory = artifact_factory
        self._machines = machines
        self._allocated_servers = []
        self._ca = ca
        self._mediaserver_deb = mediaserver_deb

    def allocate(self, name, vm=None):
        if vm is None:
            vm = self._machines.get(name)
        port = 7001
        hostname_from_here, port_from_here = vm.ports['tcp', port]
        server = Server(
            name,
            UpstartService(vm.os_access, self._mediaserver_deb.customization.service),
            install_mediaserver(vm.os_access, self._mediaserver_deb),
            RestApi(name, hostname_from_here, port_from_here),
            vm,
            port)
        server.stop(already_stopped_ok=True)
        server.installation.cleanup_core_files()
        server.installation.cleanup_var_dir()
        server.installation.put_key_and_cert(self._ca.generate_key_and_cert())
        server.installation.restore_mediaserver_conf()
        server.installation.update_mediaserver_conf({
            'logLevel': 'DEBUG2',
            'tranLogLevel': 'DEBUG2',
            'checkForUpdateUrl': 'http://127.0.0.1:8080',  # TODO: Use fake server responding with small updates.
            })
        return server

    def release(self, server):
        if server.service.is_running() and not server.is_online():
            log.warning('Server %s is started but does not respond to ping - making core dump', server)
            server.service.make_core_dump()

        log_contents = server.installation.read_log()
        if log_contents:
            artifact_factory = self._artifact_factory(
                ['server', server.name], name='server-%s' % server.name, artifact_type=SERVER_LOG_ARTIFACT_TYPE)
            log_path = artifact_factory.produce_file_path().write_bytes(log_contents)
            log.debug('log file for server %s, %s is stored to %s', server.name, server, log_path)

        for core_dump in server.installation.list_core_dumps():
            artifact_factory = self._artifact_factory(
                ['server', server.name.lower(), core_dump.name],
                name='%s-%s' % (server.name.lower(), core_dump.name),
                is_error=True,
                artifact_type=CORE_FILE_ARTIFACT_TYPE)
            local_core_dump_path = artifact_factory.produce_file_path()
            server.machine.os_access.get_file(core_dump, local_core_dump_path)
            traceback = create_core_file_traceback(
                server.machine.os_access,
                server.installation.binary,
                server.dir / 'lib',
                core_dump)
            artifact_factory = self._artifact_factory(
                ['server', server.name.lower(), core_dump.name, 'traceback'],
                name='%s-%s-tb' % (server.name.lower(), core_dump.name),
                is_error=True,
                artifact_type=TRACEBACK_ARTIFACT_TYPE)
            local_traceback_path = artifact_factory.produce_file_path()
            local_traceback_path.write_bytes(traceback)
            log.warning('Core dump on %r: %s, %s.', server.name, server, local_core_dump_path, local_traceback_path)
