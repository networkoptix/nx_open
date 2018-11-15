"""Work with mediaserver as single entity: start/stop, setup, configure, access HTTP api, storage, etc..."""

import datetime
import logging
from abc import ABCMeta, abstractmethod

from framework.installation.installation import Installation
from framework.installation.installer import InstallerSet
from framework.installation.make_installation import make_installation
from framework.installation.storage import Storage
from framework.mediaserver_api import MediaserverApi
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import copy_file
from framework.waiting import wait_for_truthy
from ..context_logger import context_logger

MEDIASERVER_STORAGE_PATH = 'var/data'

MEDIASERVER_CREDENTIALS_TIMEOUT = datetime.timedelta(minutes=5)
MEDIASERVER_MERGE_TIMEOUT = MEDIASERVER_CREDENTIALS_TIMEOUT  # timeout for local system ids become the same
MEDIASERVER_MERGE_REQUEST_TIMEOUT = datetime.timedelta(seconds=90)  # timeout for mergeSystems REST api request
MEDIASERVER_START_TIMEOUT = datetime.timedelta(minutes=2)  # timeout when waiting for server become online (pingable)

_logger = logging.getLogger(__name__)


@context_logger(_logger, 'framework.waiting')
@context_logger(_logger, 'framework.http_api')
@context_logger(_logger, 'framework.mediaserver_api')
class BaseMediaserver(object):
    __metaclass__ = ABCMeta

    def __init__(self, name, installation):  # type: (str, Installation) -> None
        self.name = name
        self.installation = installation

    @abstractmethod
    def is_online(self):
        pass

    def start(self, already_started_ok=False):
        _logger.info('Start %s', self)
        service = self.installation.service
        if service.is_running():
            if not already_started_ok:
                raise Exception("Already started")
        else:
            service.start()
            wait_for_truthy(
                self.is_online,
                description='{} is started'.format(self),
                timeout_sec=MEDIASERVER_START_TIMEOUT.total_seconds(),
                )

    def stop(self, already_stopped_ok=False):
        _logger.info("Stop mediaserver %s.", self)
        service = self.installation.service
        if service.is_running():
            service.stop()
            wait_for_truthy(lambda: not service.is_running(), "{} stops".format(service))
        else:
            if not already_stopped_ok:
                raise Exception("Already stopped")

    def make_core_dump_if_running(self):
        status = self.installation.service.status()
        if status.is_running:
            self.installation.os_access.make_core_dump(status.pid)

    def examine(self, stopped_ok=False):
        examination_logger = _logger.getChild('examination')
        examination_logger.info('Post-test check for %s', self)
        status = self.installation.service.status()
        if status.is_running:
            examination_logger.debug("%s is running.", self)
            if self.is_online():
                examination_logger.debug("%s is online.", self)
            else:
                self.installation.os_access.make_core_dump(status.pid)
                _logger.error('{} is not online; core dump made.'.format(self))
        else:
            if stopped_ok:
                examination_logger.info("%s is stopped; it's OK.", self)
            else:
                _logger.error("{} is stopped.".format(self))

    def has_core_dumps(self):
        return self.installation.list_core_dumps() != []

    def collect_artifacts(self, artifacts_dir):
        for file in self.installation.list_log_files():
            if file.exists():
                copy_file(file, artifacts_dir / file.name)
        for core_dump in self.installation.list_core_dumps():
            local_core_dump_path = artifacts_dir / core_dump.name
            copy_file(core_dump, local_core_dump_path)
            # noinspection PyBroadException
            try:
                traceback = self.installation.parse_core_dump(core_dump)
                backtrace_name = local_core_dump_path.name + '.backtrace.txt'
                local_traceback_path = local_core_dump_path.with_name(backtrace_name)
                local_traceback_path.write_text(traceback)
            except Exception:
                _logger.exception("Cannot parse core dump: %s.", core_dump)


class Mediaserver(BaseMediaserver):
    """Mediaserver, same for physical and virtual machines"""

    def __init__(self, name, installation, port=7001):
        # type: (str, Installation, int) -> None
        super(Mediaserver, self).__init__(name, installation)
        assert port is not None
        self.name = name
        self.os_access = installation.os_access
        self.port = port
        self.service = installation.service
        forwarded_port = installation.os_access.port_map.remote.tcp(self.port)
        forwarded_address = installation.os_access.port_map.remote.address
        self.api = MediaserverApi(
            '{}:{}'.format(forwarded_address, forwarded_port),
            alias=name, specific_features=installation.specific_features())

    def __str__(self):
        return 'Mediaserver {} at {}'.format(self.name, self.api.generic.http.url(''))

    def __repr__(self):
        return '<{!s}>'.format(self)

    @classmethod
    def setup(cls, os_access, installer_set, ssl_key_cert):
        # type: (OSAccess, InstallerSet, str) -> Mediaserver
        """Get mediaserver as if it hasn't run before."""
        installation = make_installation(os_access, installer_set.customization)
        installer = installation.choose_installer(installer_set.installers)
        installation.install(installer)
        mediaserver = cls(os_access.alias, installation)
        mediaserver.stop(already_stopped_ok=True)
        mediaserver.installation.cleanup(ssl_key_cert)
        return mediaserver

    def is_online(self):
        return self.api.is_online()

    @property
    def storage(self):
        """Any non-backup storage"""

        def get_storage():
            response = self.api.generic.get('ec2/getStorages')
            for storage_data in response:
                if not storage_data['isBackup']:
                    return Storage(self.os_access, self.os_access.path_cls(storage_data['url']))

        return wait_for_truthy(get_storage)
