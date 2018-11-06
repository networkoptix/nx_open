import logging
import string
from abc import ABCMeta, abstractproperty
from functools import wraps
from io import BytesIO

from netaddr import IPAddress
from pathlib2 import PureWindowsPath
from smb.SMBConnection import SMBConnection
from smb.base import SharedFile
from smb.smb_structs import OperationFailure

from framework.method_caching import cached_getter
from framework.os_access import exceptions
from framework.os_access.path import FileSystemPath
from framework.waiting import Wait

# See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
_STATUS_SUCCESS = 0x00000000
_STATUS_NO_SUCH_FILE = 0xC000000F
_STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034
_STATUS_OBJECT_NAME_COLLISION = 0xC0000035
_STATUS_OBJECT_PATH_NOT_FOUND = 0xC000003A
_STATUS_NOT_A_DIRECTORY = 0xC0000103
_STATUS_FILE_IS_A_DIRECTORY = 0xC00000BA
_STATUS_SHARING_VIOLATION = 0xC0000043
_STATUS_DELETE_PENDING = 0xC0000056
_STATUS_DIRECTORY_NOT_EMPTY = 0xC0000101
_STATUS_REQUEST_NOT_ACCEPTED = 0xC00000D0

_logger = logging.getLogger(__name__)


class RequestNotAccepted(Exception):
    message = (
        "No more connections can be made to this remote computer at this time"
        " because the computer has already accepted the maximum number of connections.")

    def __init__(self):
        super(RequestNotAccepted, self).__init__(self.message)


def _reraising_on_operation_failure(status_to_error_cls):
    def decorator(func):
        @wraps(func)
        def decorated(self, *args, **kwargs):
            try:
                return func(self, *args, **kwargs)
            except OperationFailure as e:
                for smb_message in reversed(e.smb_messages):
                    last_message_status = smb_message.status
                    if last_message_status != _STATUS_SUCCESS:
                        break
                else:
                    raise
                if last_message_status in status_to_error_cls:
                    error_cls = status_to_error_cls[last_message_status]
                    raise error_cls(e)
                raise

        return decorated

    return decorator


def _retrying_on_status(*statuses):
    def decorator(func):
        @wraps(func)
        def decorated(*args, **kwargs):
            until = "no {} error".format(', '.join(map(hex, statuses)))
            wait = Wait(until, timeout_sec=5)
            while True:
                try:
                    return func(*args, **kwargs)
                except OperationFailure as e:
                    last_message_status = e.smb_messages[-1].status
                    if last_message_status not in statuses:
                        raise
                    if not wait.again():
                        raise
                wait.sleep()

        return decorated

    return decorator


class SMBConnectionPool(object):
    def __init__(
            self,
            username, password,
            direct_smb_address, direct_smb_port,
            ):
        self._username = username  # str
        self._password = password  # str
        self._direct_smb_address = direct_smb_address  # type: IPAddress
        self._direct_smb_port = direct_smb_port  # type: int

    def __repr__(self):
        return '<SMBConnection {!s}:{:d}>'.format(self._direct_smb_address, self._direct_smb_port)

    @cached_getter
    @_reraising_on_operation_failure({_STATUS_REQUEST_NOT_ACCEPTED: RequestNotAccepted})
    @_retrying_on_status(_STATUS_REQUEST_NOT_ACCEPTED)
    def connection(self):
        # TODO: Use connection pooling with keep-alive: connections can be closed from server side.
        # See: http://pysmb.readthedocs.io/en/latest/api/smb_SMBConnection.html (Caveats section)
        # Do not keep a SMBConnection instance "idle" for too long,
        # i.e. keeping a SMBConnection instance but not using it.
        # Most SMB/CIFS servers have some sort of keepalive mechanism and
        # impose a timeout limit. If the clients fail to respond within
        # the timeout limit, the SMB/CIFS server may disconnect the client.
        client_name = u'FUNC_TESTS_EXECUTION'.encode('ascii')  # Arbitrary ASCII string.
        server_name = 'dummy'  # Not used because SMB runs over TCP directly. But must not be empty.
        connection = SMBConnection(
            self._username, self._password,
            client_name, server_name, is_direct_tcp=True)
        connection_succeeded = connection.connect(
            str(self._direct_smb_address), port=self._direct_smb_port)
        # SMBConnectionPool has no special closing actions;
        # socket is closed when object is deleted.
        if not connection_succeeded:
            raise RuntimeError("Connection to {}:{} failed".format(
                self._direct_smb_address, self._direct_smb_port))
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

    @classmethod
    def specific_cls(cls, address, port, user_name, password):
        # TODO: Delay `%TEMP%` and `%USERPROFILE%` expansion until accessed and use them.
        class SpecificSMBPath(cls):
            _smb_connection_pool = SMBConnectionPool(user_name, password, address, port)

            @classmethod
            def home(cls):
                return cls('C:', 'Users', user_name)

        return SpecificSMBPath

    def __repr__(self):
        return '<SMBPath {!s} on {!r}>'.format(self, self._smb_connection_pool)

    @classmethod
    def tmp(cls):
        return cls('C:\\', 'Windows', 'Temp', 'FuncTests')

    @property
    def _service_name(self):
        """Name of share"""
        if not self.drive:
            raise ValueError("No drive in Windows path {}".format(self))
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

    @_retrying_on_status(_STATUS_SHARING_VIOLATION)  # Let OS and processes time unlock files.
    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: exceptions.NotAFile,
        _STATUS_OBJECT_NAME_NOT_FOUND: exceptions.DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.DoesNotExist,
        })
    def unlink(self):
        if '*' in str(self):
            raise ValueError("{!r} contains '*', but files can be deleted only by one")
        self._smb_connection_pool.connection().deleteFiles(self._service_name, self._relative_path)

    def expanduser(self):
        """Don't do any expansion on Windows"""
        return self

    @_reraising_on_operation_failure({
        _STATUS_NOT_A_DIRECTORY: exceptions.NotADir,
        _STATUS_OBJECT_NAME_NOT_FOUND: exceptions.DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.DoesNotExist,
        })
    def _glob(self, pattern):
        try:
            return self._smb_connection_pool.connection().listPath(
                self._service_name, self._relative_path,
                pattern=pattern)
        except OperationFailure as e:
            no_files_match = all([
                # TODO: Consider checking commands too.
                e.smb_messages[-1].status == _STATUS_SUCCESS,
                e.smb_messages[-2].status == _STATUS_NO_SUCH_FILE,
                ])
            if no_files_match:
                return []
            raise

    def glob(self, pattern):
        return [
            self / pysmb_file.filename
            for pysmb_file in self._glob(pattern)
            if pysmb_file.filename not in {u'.', u'..'}
            ]

    def mkdir(self, parents=False, exist_ok=False):
        # No way to create all directories (parents and self) at once.
        # Usually, only few nearest parents don't exist,
        # that's why they're checked from nearest one.
        # TODO: Consider more sophisticated algorithms.
        if self.parent == self:
            assert self == self.__class__(self.anchor)  # I.e. disk root, e.g. C:\.
            if not exist_ok:
                raise exceptions.AlreadyExists(repr(self))
        else:
            try:
                _logger.debug("Create directory %s on %s", self._relative_path, self._service_name)
                self._smb_connection_pool.connection().createDirectory(self._service_name, self._relative_path)
            except OperationFailure as e:
                last_message_status = e.smb_messages[-1].status
                # See: https://msdn.microsoft.com/en-us/library/cc704588.aspx
                if last_message_status == _STATUS_OBJECT_NAME_COLLISION:
                    if not exist_ok:
                        raise exceptions.AlreadyExists(repr(self))
                elif last_message_status == _STATUS_OBJECT_NAME_NOT_FOUND:
                    if parents:
                        self.parent.mkdir(parents=False, exist_ok=True)
                        self.mkdir(parents=False, exist_ok=False)
                    else:
                        raise exceptions.BadParent("Parent {0.parent} of {0} doesn't exist.".format(self))
                elif last_message_status == _STATUS_OBJECT_PATH_NOT_FOUND:
                    if parents:
                        self.parent.parent.mkdir(parents=True, exist_ok=True)
                        self.parent.mkdir(parents=False, exist_ok=False)
                        self.mkdir(parents=False, exist_ok=False)
                    else:
                        raise exceptions.BadParent("Grandparent {0.parent.parent} of {0} doesn't exist.".format(self))
                else:
                    raise

    @_reraising_on_operation_failure({
        _STATUS_DIRECTORY_NOT_EMPTY: exceptions.NotEmpty,
        _STATUS_OBJECT_NAME_NOT_FOUND: exceptions.DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.DoesNotExist,
        })
    def rmdir(self):
        self._smb_connection_pool.connection().deleteDirectory(self._service_name, self._relative_path)

    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: exceptions.NotAFile,
        _STATUS_OBJECT_NAME_NOT_FOUND: exceptions.DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.DoesNotExist,
        })
    def read_bytes(self, offset=0, max_length=-1):
        # TODO: Speedup. Speed is ~1.4 MB/sec. Full dumps are ~300 MB.
        # Performance is not mentioned on pysmb page.
        ad_hoc_file_object = BytesIO()
        _attributes, bytes_read = self._smb_connection_pool.connection().retrieveFileFromOffset(
            self._service_name, self._relative_path,
            ad_hoc_file_object,
            offset=offset, max_length=max_length)
        data = ad_hoc_file_object.getvalue()
        assert bytes_read == len(data)
        return data

    @_retrying_on_status(_STATUS_DELETE_PENDING)
    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: exceptions.NotAFile,
        _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.BadParent})
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

    @_reraising_on_operation_failure({
        _STATUS_OBJECT_NAME_NOT_FOUND: exceptions.DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.DoesNotExist,
        })
    def size(self):
        attributes = self._smb_connection_pool.connection().getAttributes(
            self._service_name, self._relative_path)  # type: SharedFile
        if attributes.isDirectory:
            raise exceptions.NotAFile("Attributes of the {}: {}".format(self, attributes))
        return attributes.file_size

    def symlink_to(self, target, target_is_directory=False):
        raise NotImplementedError("Symlinks over SMB are not well-supported")
