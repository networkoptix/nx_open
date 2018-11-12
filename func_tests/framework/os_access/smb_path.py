import errno
import functools
import io
import logging
import stat
import sys

import pathlib2 as pathlib
import six
from netaddr import IPAddress
from scandir import GenericDirEntry
from smb.SMBConnection import SMBConnection
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
_STATUS_ACCESS_DENIED = 0xC0000022

_status_to_errno = {
    _STATUS_NO_SUCH_FILE: errno.ENOENT,
    _STATUS_OBJECT_NAME_NOT_FOUND: errno.ENOENT,
    _STATUS_OBJECT_PATH_NOT_FOUND: errno.ENOENT,
    _STATUS_OBJECT_NAME_COLLISION: errno.EEXIST,
    _STATUS_NOT_A_DIRECTORY: errno.ENOTDIR,
    _STATUS_FILE_IS_A_DIRECTORY: errno.EISDIR,
    _STATUS_ACCESS_DENIED: errno.EACCES,
    _STATUS_SHARING_VIOLATION: 32,  # Same as if Python cannot delete file on Windows.
    _STATUS_DELETE_PENDING: errno.EWOULDBLOCK,
    _STATUS_DIRECTORY_NOT_EMPTY: errno.ENOTEMPTY,
    _STATUS_REQUEST_NOT_ACCEPTED: errno.EPERM,
    }

_logger = logging.getLogger(__name__)


def _reraising_on_operation_failure(status_to_error_cls):
    def decorator(func):
        @functools.wraps(func)
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
                    error = error_cls(_status_to_errno[last_message_status], e.message)
                    six.reraise(error_cls, error, sys.exc_info()[2])
                raise

        return decorated

    return decorator


class UsedByAnotherProcess(exceptions.FileSystemError):
    pass


_reraise_for_existing = _reraising_on_operation_failure({
    _STATUS_DIRECTORY_NOT_EMPTY: exceptions.NotEmpty,
    _STATUS_OBJECT_NAME_NOT_FOUND: exceptions.DoesNotExist,
    _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.DoesNotExist,
    _STATUS_FILE_IS_A_DIRECTORY: exceptions.NotAFile,
    _STATUS_SHARING_VIOLATION: UsedByAnotherProcess,
    })
_reraise_for_new = _reraising_on_operation_failure({
    _STATUS_FILE_IS_A_DIRECTORY: exceptions.NotAFile,
    _STATUS_OBJECT_PATH_NOT_FOUND: exceptions.BadParent,
    _STATUS_OBJECT_NAME_COLLISION: exceptions.AlreadyExists,
    })


def _retrying_on_status(*statuses):
    def decorator(func):
        @functools.wraps(func)
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


class RequestNotAccepted(Exception):
    message = (
        "No more connections can be made to this remote computer at this time"
        " because the computer has already accepted the maximum number of connections.")

    def __init__(self):
        super(RequestNotAccepted, self).__init__(self.message)


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


class _SmbStat(object):
    def __init__(self, attrs):
        self.st_mode = stat.S_IFDIR if attrs.isDirectory else stat.S_IFREG
        self.st_size = attrs.file_size


class _SmbAccessor(object):
    def __init__(self, connection_pool):
        self._smb_connection_pool = connection_pool

    def _conn(self):  # type: () -> SMBConnection
        return self._smb_connection_pool.connection()

    @_reraise_for_existing
    def stat(self, path):
        attrs = self._conn().getAttributes(path.smb_share, path.smb_path)
        return _SmbStat(attrs)

    lstat = stat

    @_reraise_for_existing
    def scandir(self, path):
        entries = []
        for shared_file in self._conn().listPath(path.smb_share, path.smb_path):
            if shared_file.filename in ('.', '..'):
                continue
            entry = GenericDirEntry(str(path), shared_file.filename)
            entry._lstat = entry._stat = _SmbStat(shared_file)
            entries.append(entry)
        return entries

    def listdir(self, path):
        return [entry.name for entry in self.scandir(path)]

    @_reraise_for_new
    def mkdir(self, path, mode=None):
        if mode is not None:
            _logger.warning("`mode` is ignored but given: %r", mode)
        self._conn().createDirectory(path.smb_share, path.smb_path)

    @_reraise_for_existing
    def rmdir(self, path):
        self._conn().deleteDirectory(path.smb_share, path.smb_path)

    @_reraise_for_existing
    def rename(self, path, new_path):
        assert new_path.smb_share == path.smb_share
        return self._conn().rename(path.smb_share, path.smb_path, new_path.smb_path)

    @_retrying_on_status(_STATUS_SHARING_VIOLATION)  # Let OS and processes time unlock files.
    @_reraise_for_existing
    def unlink(self, path):
        return self._conn().deleteFiles(path.smb_share, path.smb_path)


class SMBPath(FileSystemPath):
    _accessor = None
    _connection_pool = None  # type: SMBConnectionPool

    @classmethod
    def specific_cls(cls, hostname, port, username, password):
        class ModernSmbPath(cls):
            _connection_pool = SMBConnectionPool(username, password, hostname, port)
            _accessor = _SmbAccessor(_connection_pool)
            _flavour = pathlib._windows_flavour

            @classmethod
            def home(cls):
                return cls('C:', 'Users', username)

        return ModernSmbPath

    def _init(self, template=None):
        super(SMBPath, self)._init(template=template)
        if self._drv:
            self.smb_share = self._drv[0] + '$'
            self.smb_path = '\\'.join(self._parts[1:])

    @classmethod
    def tmp(cls):
        return cls('C:\\', 'Windows', 'Temp', 'FuncTests')

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        raise NotImplementedError(
            "Opening of file via SMB is not supported at the moment. "
            "While opening a file (create message) is essential part of SMB, "
            "`pysmb` doesn't support this.")

    @_reraise_for_existing
    def read_bytes(self):
        # TODO: Speedup. Speed is ~1.4 MB/sec. Full dumps are ~300 MB.
        # Performance is not mentioned on pysmb page.
        ad_hoc_file_object = io.BytesIO()
        _attributes, bytes_read = self._connection_pool.connection().retrieveFile(
            self.smb_share, self.smb_path, ad_hoc_file_object)
        data = ad_hoc_file_object.getvalue()
        assert bytes_read == len(data)
        return data

    @_retrying_on_status(_STATUS_DELETE_PENDING)
    @_reraise_for_new
    def write_bytes(self, data):
        ad_hoc_file_object = io.BytesIO(data)
        return self._connection_pool.connection().storeFile(
            self.smb_share, self.smb_path,
            ad_hoc_file_object)

    @_reraise_for_new
    def read_text(self, encoding='utf8', errors='strict'):
        data = self.read_bytes()
        text = data.decode(encoding=encoding, errors=errors)
        return text

    @_retrying_on_status(_STATUS_DELETE_PENDING)
    @_reraise_for_new
    def write_text(self, text, encoding='utf8', errors='strict'):
        data = text.encode(encoding=encoding, errors=errors)
        bytes_written = self.write_bytes(data)
        assert bytes_written == len(data)
        return len(text)

    @_reraise_for_new
    def yank(self, offset, max_length=None):
        # TODO: Speedup. Speed is ~1.4 MB/sec. Full dumps are ~300 MB.
        # Performance is not mentioned on pysmb page.
        ad_hoc_file_object = io.BytesIO()
        _attributes, bytes_read = self._connection_pool.connection().retrieveFileFromOffset(
            self.smb_share, self.smb_path, ad_hoc_file_object,
            offset=offset, max_length=max_length if max_length is not None else -1)
        data = ad_hoc_file_object.getvalue()
        assert bytes_read == len(data)
        return data

    @_retrying_on_status(_STATUS_DELETE_PENDING)
    @_reraise_for_new
    def patch(self, offset, data):
        ad_hoc_file_object = io.BytesIO(data)
        return self._connection_pool.connection().storeFileFromOffset(
            self.smb_share, self.smb_path, ad_hoc_file_object,
            offset=offset)
