# coding=utf-8
import logging
import os
from string import whitespace

import pytest

from framework.os_access.exceptions import BadParent, DoesNotExist, NotADir, NotAFile
from framework.os_access.local_path import LocalPath
from framework.os_access.local_shell import local_shell
from framework.os_access.path import copy_file
from framework.os_access.posix_shell_path import PosixShellPath

_logger = logging.getLogger(__name__)


@pytest.fixture()
def local_path_cls():
    return LocalPath


@pytest.fixture()
def ssh_path_cls():
    return PosixShellPath.specific_cls(local_shell)


@pytest.fixture(scope='session')
def windows_vm(vm_types):
    with vm_types['windows'].vm_ready('paths-test') as windows_vm:
        yield windows_vm


@pytest.fixture()
def smb_path_cls(windows_vm):
    return windows_vm.os_access.Path


@pytest.fixture(params=['local_path_cls', 'ssh_path_cls', 'smb_path_cls'])
def path_cls(request):
    return request.getfixturevalue(request.param)


@pytest.fixture()
def dirty_remote_test_dir(request, path_cls):
    """Just a name, cleaned up by test"""
    # See: https://docs.pytest.org/en/latest/proposals/parametrize_with_fixtures.html
    base_remote_dir = path_cls.tmp() / (__name__ + '-remote')
    return base_remote_dir / request.node.name


def _cleanup_dir(dir):
    # To avoid duplication but makes setup and test distinct.
    dir.rmtree(ignore_errors=True)
    dir.mkdir(parents=True)  # Its parents may not exist.
    return dir


@pytest.fixture()
def remote_test_dir(dirty_remote_test_dir):
    _cleanup_dir(dirty_remote_test_dir)
    return dirty_remote_test_dir


@pytest.fixture()
def local_test_dir():
    path = LocalPath.tmp() / 'test_path-local'
    path.mkdir(parents=True, exist_ok=True)
    return path


@pytest.fixture()
def existing_remote_file(remote_test_dir, existing_remote_file_size=65537):
    path = remote_test_dir / 'existing_file'
    path.write_bytes(os.urandom(existing_remote_file_size))
    return path


@pytest.fixture()
def existing_remote_dir(remote_test_dir):
    path = remote_test_dir / 'existing_dir'
    path.mkdir()
    return path


def test_tmp(path_cls):
    assert path_cls.tmp().exists()


def test_home(path_cls):
    assert path_cls.home().exists()


def test_rmtree_write_exists(dirty_remote_test_dir):
    _cleanup_dir(dirty_remote_test_dir)
    touched_file = dirty_remote_test_dir / 'touched.empty'
    assert not touched_file.exists()
    touched_file.write_bytes(b'')
    assert touched_file.exists()


@pytest.mark.parametrize('depth', [1, 2, 3, 4], ids='depth_{}'.format)
def test_rmtree_mkdir_exists(dirty_remote_test_dir, depth):
    _cleanup_dir(dirty_remote_test_dir)
    root_dir = dirty_remote_test_dir / 'root'
    root_dir.mkdir()
    target_dir = root_dir.joinpath(*['level_{}'.format(level) for level in range(1, depth + 1)])
    assert not target_dir.exists()
    with pytest.raises(DoesNotExist):
        target_dir.rmtree(ignore_errors=False)  # Not exists, raise.
    target_dir.rmtree(ignore_errors=True)  # No effect even if parent doesn't exist.
    if depth == 1:
        target_dir.mkdir(parents=False)
    else:
        with pytest.raises(BadParent):
            target_dir.mkdir(parents=False)
        target_dir.mkdir(parents=True)
    assert target_dir.exists()
    target_file = target_dir.joinpath('deep_file')
    assert not target_file.exists()
    target_file.write_bytes(b'')
    assert target_file.exists()
    root_dir.rmtree()
    assert not root_dir.exists()


_tricky_bytes = [
    ('chr0_to_chr255', bytes(bytearray(range(0x100)))),
    ('chr0_to_chr255_100times', bytes(bytearray(range(0x100)) * 100)),
    ('whitespace', whitespace),
    ('windows_newlines', '\r\n' * 100),
    ('linux_newlines', '\n' * 100),
    ('ending_with_chr0', 'abc\0'),
    ]


@pytest.mark.parametrize('data', _tricky_bytes, ids='{0[0]}'.format)
def test_write_read_bytes(remote_test_dir, data):
    name, written = data
    file_path = remote_test_dir / '{}.dat'.format(name)
    bytes_written = file_path.write_bytes(written)
    assert bytes_written == len(written)
    read = file_path.read_bytes()
    assert read == written


@pytest.mark.parametrize('data', _tricky_bytes, ids='{0[0]}'.format)
def test_write_read_tricky_bytes_with_offsets(remote_test_dir, data):
    name, written = data
    file_path = remote_test_dir / '{}.dat'.format(name)
    file_path.write_bytes(written, offset=10)
    assert file_path.read_bytes(offset=10, max_length=100) == written[:100]


def test_write_read_bytes_with_tricky_offsets(remote_test_dir):
    file_path = remote_test_dir / 'abc.dat'
    file_path.write_bytes(b'aaaaa')
    file_path.write_bytes(b'ccccc', offset=10)
    file_path.write_bytes(b'bbbbb', offset=5)
    assert file_path.read_bytes(offset=12, max_length=5) == b'ccc'
    assert file_path.read_bytes(offset=8, max_length=5) == b'bbccc'
    assert file_path.read_bytes(offset=3, max_length=5) == b'aabbb'


@pytest.mark.parametrize(
    'data',
    [
        ('latin', 'ascii', u"Two wrongs don't make a right.\nThe pen is mightier than the sword."),
        ('latin', 'utf8', u"Alla sätt är bra utom de dåliga.\nGräv där du står."),
        ('cyrillic', 'utf8', u"А дело бывало — и коза волка съедала.\nАзбука — к мудрости ступенька."),
        ('cyrillic', 'utf16', u"Загляне сонца і ў наша аконца.\nКаб свiнне poгi - нiкому б не было дарогi."),
        ('pinyin', 'utf8', u"防人之心不可無。\n福無重至,禍不單行。"),
        ('pinyin', 'utf16', u"[与其]临渊羡鱼，不如退而结网。\n君子之心不胜其小，而气量涵盖一世。"),
        ],
    ids='{0[0]}_{0[1]}'.format)
def test_write_read_text(remote_test_dir, data):
    name, encoding, written = data
    file_path = remote_test_dir / '{}_{}.txt'.format(name, encoding)
    chars_written = file_path.write_text(written, encoding=encoding)
    assert chars_written == len(written)
    read = file_path.read_text(encoding=encoding)
    assert read == written


def test_write_to_dir(existing_remote_dir):
    with pytest.raises(NotAFile):
        existing_remote_dir.write_bytes(os.urandom(1000))


@pytest.fixture(params=[1, 2, 3], ids='depth_{}'.format)
def path_with_file_in_parents(request, existing_remote_file):
    path = existing_remote_file
    for level in range(1, request.param + 1):
        path /= 'level_{}'.format(level)
    return path


def test_write_when_parent_is_a_file(path_with_file_in_parents):
    with pytest.raises(BadParent):
        path_with_file_in_parents.write_bytes(b'anything')


def test_mkdir_when_parent_is_a_file(path_with_file_in_parents):
    with pytest.raises(BadParent):
        path_with_file_in_parents.mkdir()


def test_read_from_dir(existing_remote_dir):
    with pytest.raises(NotAFile):
        _ = existing_remote_dir.read_bytes()


def test_unlink_dir(existing_remote_dir):
    with pytest.raises(NotAFile):
        existing_remote_dir.unlink()


def test_write_to_existing_file(existing_remote_file):
    data = os.urandom(1000)
    bytes_written = existing_remote_file.write_bytes(data)
    assert bytes_written == len(data)


def test_read_from_non_existent(remote_test_dir):
    non_existent_file = remote_test_dir / 'non_existent'
    with pytest.raises(DoesNotExist):
        _ = non_existent_file.read_bytes()


def test_glob_on_file(existing_remote_file):
    with pytest.raises(NotADir):
        _ = list(existing_remote_file.glob('*'))


def test_glob_on_non_existent(existing_remote_dir):
    non_existent_path = existing_remote_dir / 'non_existent'
    with pytest.raises(DoesNotExist):
        _ = list(non_existent_path.glob('*'))


def test_glob_on_empty_dir(existing_remote_dir):
    assert not list(existing_remote_dir.glob('*'))


def test_glob_no_result(existing_remote_dir):
    existing_remote_dir.joinpath('oi.existing').write_bytes(b'empty')
    assert not list(existing_remote_dir.glob('*.non_existent'))


@pytest.mark.parametrize('iterations', [2, 10], ids='{}iterations'.format)
@pytest.mark.parametrize('depth', [2, 10], ids='depth{}'.format)
def test_many_mkdir_rmtree(remote_test_dir, iterations, depth):
    """Sometimes mkdir after rmtree fails because of pending delete operations"""
    top_path = remote_test_dir / 'top'
    deep_path = top_path.joinpath(*('level{}'.format(level) for level in range(depth)))
    for _ in range(iterations):
        deep_path.mkdir(parents=True)
        deep_path.joinpath('treasure').write_bytes(b'\0' * 1000000)
        top_path.rmtree()


path_type_list = ['local', 'posix', 'ssh', 'smb']

def remote_file_path(request, path_type, name):
    if path_type == 'ssh':
        vm_fixture_name = 'linux_vm'
    if path_type == 'smb':
        vm_fixture_name = 'windows_vm'
    vm = request.getfixturevalue(vm_fixture_name)
    path_class = vm.os_access.Path
    base_remote_dir = path_class.tmp().joinpath(__name__ + '-remote')
    return base_remote_dir.joinpath(request.node.name + '-' + name)

def path_type_to_path(request, node_dir, ssh_path_cls, path_type, name):
    if path_type in 'local':
        path = node_dir.joinpath('local-file-%s' % name)
    elif path_type == 'posix':
        path = ssh_path_cls(str(node_dir.joinpath('posix-file-%s' % name)))
    else:
        path = remote_file_path(request, path_type, name)
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


@pytest.mark.parametrize('destination_path_type', path_type_list)
@pytest.mark.parametrize('source_path_type', path_type_list)
def test_copy_file(request, node_dir, ssh_path_cls, source_path_type, destination_path_type):
    source = path_type_to_path(request, node_dir, ssh_path_cls, source_path_type, 'source')
    destination = path_type_to_path(request, node_dir, ssh_path_cls, destination_path_type, 'destination')

    _logger.info('Copy: %r -> %r', source, destination)

    bytes = '0123456789' * 10 * 1024  # 100K
    source.write_bytes(bytes)
    destination.ensure_file_is_missing()
    copy_file(source, destination)
    assert destination.exists()
    assert destination.read_bytes() == bytes
