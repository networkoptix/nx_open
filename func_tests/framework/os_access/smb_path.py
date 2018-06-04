import logging
import string
from abc import ABCMeta, abstractproperty
from contextlib import closing
from functools import wraps
from io import BytesIO

from netaddr import IPAddress
from nmb.NetBIOS import NetBIOS
from pathlib2 import PureWindowsPath
from smb.SMBConnection import SMBConnection
from smb.smb_structs import OperationFailure

from framework.method_caching import cached_getter
from framework.os_access.exceptions import AlreadyExists, BadParent, DoesNotExist, NotADir, NotAFile
from framework.os_access.path import FileSystemPath

_STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034  # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
_STATUS_OBJECT_NAME_COLLISION = 0xC0000035  # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
_STATUS_OBJECT_PATH_NOT_FOUND = 0xC000003A  # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
_STATUS_NOT_A_DIRECTORY = 0xC0000103  # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
_STATUS_FILE_IS_A_DIRECTORY = 0xC00000BA  # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx

_logger = logging.getLogger(__name__)


def _reraising_on_operation_failure(status_to_error_cls):
    def decorator(func):
        @wraps(func)
        def decorated(self, *args, **kwargs):
            try:
                return func(self, *args, **kwargs)
            except OperationFailure as e:
                last_message_status = e.smb_messages[-1].status
                if last_message_status in status_to_error_cls:
                    error_cls = status_to_error_cls[last_message_status]
                    raise error_cls(e)
                raise

        return decorated

    return decorator


class SMBConnectionPool(object):
    def __init__(
            self,
            username, password,
            name_service_address, name_service_port,
            session_service_address, session_service_port,
            ):
        self._username = username  # str
        self._password = password  # str
        self._name_service_address = name_service_address  # type: IPAddress
        self._name_service_port = name_service_port  # type: int
        self._session_service_address = session_service_address  # type: IPAddress
        self._session_service_port = session_service_port  # type: int

    @cached_getter
    def _net_bios_name(self):
        with closing(NetBIOS(broadcast=False)) as client:
            server_name = client.queryIPForName(
                str(self._name_service_address), port=self._name_service_port)
            # TODO: Check and retry when got None.
        return server_name[0]

    @cached_getter
    def connection(self):
        # TODO: Use connection pooling with keep-alive: connections can be closed from server side.
        # See: http://pysmb.readthedocs.io/en/latest/api/smb_SMBConnection.html (Caveats section)
        # Do not keep a SMBConnection instance "idle" for too long,
        # i.e. keeping a SMBConnection instance but not using it.
        # Most SMB/CIFS servers have some sort of keepalive mechanism and
        # impose a timeout limit. If the clients fail to respond within
        # the timeout limit, the SMB/CIFS server may disconnect the client.
        client_name = u'FUNC_TESTS_EXECUTION'.encode('ascii')  # Arbitrary ASCII string.
        server_name = self._net_bios_name()
        connection = SMBConnection(
            self._username, self._password,
            client_name, server_name)
        connection_succeeded = connection.connect(
            str(self._session_service_address), port=self._session_service_port)
        # SMBConnectionPool has no special closing actions;
        # socket is closed when object is deleted.
        if not connection_succeeded:
            raise RuntimeError("Connection to {}:{} failed".format(
                self._session_service_address, self._session_service_port))
        return connection


class SMBPath(FileSystemPath, PureWindowsPath):
    """Base class for file system access through SMB (v2) protocol

    It's the simplest way to integrate with `pathlib` and `pathlib2`.
    When manipulating with paths, `pathlib` doesn't call neither
    `__new__` nor `__init__`. The only information preserved is type.
    That's why `SMB` credentials, ports and address are in class class,
    not to object. This class is not on a module level, therefore,
    it will be referenced by `WindowsAccess` object and by path objects
    and will live until those objects live.
    """
    __metaclass__ = ABCMeta
    _smb_connection_pool = abstractproperty()  # type: SMBConnectionPool

    @property
    def _service_name(self):
        """Name of share"""
        drive_letter = self.drive[0].upper()
        assert drive_letter in string.ascii_uppercase
        service_name = u'{}$'.format(drive_letter)  # C$, D$, ...
        return service_name

    @property
    def _relative_path(self):
        """Path relative to share root"""
        rel_path = self.relative_to(self.anchor)
        rel_path_str = str(rel_path).encode().decode()  # Portable unicode.
        assert not rel_path_str.startswith('\\')
        return rel_path_str

    def exists(self):
        try:
            _ = self._smb_connection_pool.connection().getAttributes(self._service_name, self._relative_path)
        except OperationFailure as e:
            last_message_status = e.smb_messages[-1].status
            if last_message_status == _STATUS_OBJECT_NAME_NOT_FOUND:
                return False
            if last_message_status == _STATUS_OBJECT_PATH_NOT_FOUND:
                return False
            raise
        return True

    @_reraising_on_operation_failure({_STATUS_FILE_IS_A_DIRECTORY: NotAFile})
    def unlink(self):
        if '*' in str(self):
            raise ValueError("{!r} contains '*', but files can be deleted only by one")
        self._smb_connection_pool.connection().deleteFiles(self._service_name, self._relative_path)

    def expanduser(self):
        """Don't do any expansion on Windows"""
        return self

    @_reraising_on_operation_failure({
        _STATUS_NOT_A_DIRECTORY: NotADir,
        _STATUS_OBJECT_NAME_NOT_FOUND: DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: DoesNotExist,
        })
    def _glob(self, pattern):
        return self._smb_connection_pool.connection().listPath(
            self._service_name, self._relative_path,
            pattern=pattern)

    def glob(self, pattern):
        return [
            self / pysmb_file.filename
            for pysmb_file in self._glob(pattern)
            if pysmb_file.filename not in {u'.', u'..'}
            ]

    def mkdir(self, parents=False, exist_ok=True):
        # No way to create all directories (parents and self) at once.
        # Usually, only few nearest parents don't exist,
        # that's why they're checked from nearest one.
        # TODO: Consider more sophisticated algorithms.
        if self.parent == self:
            assert self == self.__class__(self.anchor)  # I.e. disk root, e.g. C:\.
            if not exist_ok:
                raise AlreadyExists(repr(self))
        else:
            try:
                _logger.debug("Create directory %s on %s", self._relative_path, self._service_name)
                self._smb_connection_pool.connection().createDirectory(self._service_name, self._relative_path)
            except OperationFailure as e:
                last_message_status = e.smb_messages[-1].status
                # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
                if last_message_status == _STATUS_OBJECT_NAME_COLLISION:
                    if not exist_ok:
                        raise AlreadyExists(repr(self))
                elif last_message_status == _STATUS_OBJECT_NAME_NOT_FOUND:
                    if parents:
                        self.parent.mkdir(parents=False, exist_ok=True)
                        self.mkdir(parents=False, exist_ok=False)
                    else:
                        raise BadParent("Parent {0.parent} of {0} doesn't exist.".format(self))
                elif last_message_status == _STATUS_OBJECT_PATH_NOT_FOUND:
                    if parents:
                        self.parent.parent.mkdir(parents=True, exist_ok=True)
                        self.parent.mkdir(parents=False, exist_ok=False)
                        self.mkdir(parents=False, exist_ok=False)
                    else:
                        raise BadParent("Grandparent {0.parent.parent} of {0} doesn't exist.".format(self))
                else:
                    raise

    def rmtree(self, ignore_errors=False):
        try:
            iter_entries = self.glob('*')
        except DoesNotExist:
            if ignore_errors:
                pass
            else:
                raise
        except NotADir:
            self.unlink()
        else:
            for entry in iter_entries:
                entry.rmtree()
            self._smb_connection_pool.connection().deleteDirectory(self._service_name, self._relative_path)

    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: NotAFile,
        _STATUS_OBJECT_NAME_NOT_FOUND: DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: DoesNotExist,
        })
    def read_bytes(self):
        ad_hoc_file_object = BytesIO()
        _attributes, bytes_read = self._smb_connection_pool.connection().retrieveFile(
            self._service_name, self._relative_path,
            ad_hoc_file_object)
        data = ad_hoc_file_object.getvalue()
        assert bytes_read == len(data)
        return data

    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: NotAFile,
        _STATUS_OBJECT_PATH_NOT_FOUND: BadParent})
    def write_bytes(self, data, offset=None):
        ad_hoc_file_object = BytesIO(data)
        if offset is None:
            return self._smb_connection_pool.connection().storeFile(
                self._service_name, self._relative_path,
                ad_hoc_file_object)
        else:
            return self._smb_connection_pool.connection().storeFileFromOffset(
                self._service_name, self._relative_path,
                ad_hoc_file_object,
                offset=offset)

    def read_text(self, encoding='ascii', errors='strict'):
        data = self.read_bytes()
        text = data.decode(encoding=encoding, errors=errors)
        return text

    def write_text(self, text, encoding='ascii', errors='strict'):
        data = text.encode(encoding=encoding, errors=errors)
        bytes_written = self.write_bytes(data)
        assert bytes_written == len(data)
        return len(text)
