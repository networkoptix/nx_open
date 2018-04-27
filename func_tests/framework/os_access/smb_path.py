import logging
import string
from abc import ABCMeta, abstractproperty
from contextlib import closing
from functools import wraps
from io import BytesIO

from nmb.NetBIOS import NetBIOS
from pathlib2 import PureWindowsPath
from pylru import lrudecorator
from smb.SMBConnection import SMBConnection
from smb.smb_structs import OperationFailure

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


class SMBPath(FileSystemPath, PureWindowsPath):
    __metaclass__ = ABCMeta
    _username, _password = abstractproperty(), abstractproperty()
    _name_service_address, _name_service_port = abstractproperty(), abstractproperty()
    _session_service_address, _session_service_port = abstractproperty(), abstractproperty()

    @classmethod
    @lrudecorator(1)
    def _net_bios_name(cls):
        with closing(NetBIOS(broadcast=False)) as client:
            server_name = client.queryIPForName(
                str(cls._name_service_address), port=cls._name_service_port)
        return server_name[0]

    @classmethod
    @lrudecorator(1)
    def _connection(cls):
        client_name = u'FUNC_TESTS_EXECUTION'.encode('ascii')  # Arbitrary ASCII string.
        server_name = cls._net_bios_name()
        connection = SMBConnection(
            cls._username, cls._password,
            client_name, server_name)
        connection_succeeded = connection.connect(
            str(cls._session_service_address), port=cls._session_service_port)
        if not connection_succeeded:
            raise RuntimeError("Connection to {}:{} failed".format(
                cls._session_service_address, cls._session_service_port))
        return connection

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
            _ = self._connection().getAttributes(self._service_name, self._relative_path)
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
        self._connection().deleteFiles(self._service_name, self._relative_path)

    def expanduser(self):
        """Don't do any expansion on Windows"""
        return self

    @_reraising_on_operation_failure({
        _STATUS_NOT_A_DIRECTORY: NotADir,
        _STATUS_OBJECT_NAME_NOT_FOUND: DoesNotExist,
        _STATUS_OBJECT_PATH_NOT_FOUND: DoesNotExist,
        })
    def _glob(self, pattern):
        return self._connection().listPath(
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
                self._connection().createDirectory(self._service_name, self._relative_path)
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
            self._connection().deleteDirectory(self._service_name, self._relative_path)

    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: NotAFile,
        _STATUS_OBJECT_NAME_NOT_FOUND: DoesNotExist,
        })
    def read_bytes(self):
        ad_hoc_file_object = BytesIO()
        _attributes, bytes_read = self._connection().retrieveFile(
            self._service_name, self._relative_path,
            ad_hoc_file_object)
        data = ad_hoc_file_object.getvalue()
        assert bytes_read == len(data)
        return data

    @_reraising_on_operation_failure({
        _STATUS_FILE_IS_A_DIRECTORY: NotAFile,
        _STATUS_OBJECT_PATH_NOT_FOUND: BadParent})
    def write_bytes(self, data):
        ad_hoc_file_object = BytesIO(data)
        return self._connection().storeFile(
            self._service_name, self._relative_path,
            ad_hoc_file_object)

    def read_text(self, encoding='ascii', errors='strict'):
        data = self.read_bytes()
        text = data.decode(encoding=encoding, errors=errors)
        return text

    def write_text(self, text, encoding='ascii', errors='strict'):
        data = text.encode(encoding=encoding, errors=errors)
        self.write_bytes(data)
