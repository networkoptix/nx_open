# coding=utf-8
import filecmp
import os
from string import whitespace

import pytest
from pathlib2 import Path

pytest_plugins = ['fixtures.ad_hoc_ssh']


@pytest.fixture(params=['ssh'])
def base_dir(request, ad_hoc_ssh_access):
    # Creation is checked in SSHAccess tests.
    if request.param == 'ssh':
        return ad_hoc_ssh_access.Path('/tmp/func_tests/paths_test_sandbox')


def prepare_sandbox_path(base_dir, file_name):
    file_path = base_dir / file_name
    file_path.parent.mkdir(exist_ok=True, parents=True)
    assert file_path.parent.exists()
    if file_path.exists():
        file_path.unlink()
    return file_path


def test_unlink_touch_exists(base_dir):
    path = prepare_sandbox_path(base_dir, 'touched.empty')
    assert not path.exists()
    path.touch(exist_ok=False)
    assert path.exists()
    path.touch(exist_ok=True)
    with pytest.raises(Exception):
        path.touch(exist_ok=False)


@pytest.mark.parametrize(
    'data',
    [
        ('chr0_to_chr255', b''.join(map(chr, range(0xFF)))),
        ('chr0_to_chr255_100times', b''.join(map(chr, range(0xFF))) * 100),
        ('whitespace', whitespace),
        ('windows_newlines', '\r\n' * 100),
        ('linux_newlines', '\n' * 100),
        ('ending_with_chr0', 'abc\0'),
        ],
    ids=lambda data: data[0])
def test_write_read_bytes(data, base_dir):
    name, written = data
    file_path = prepare_sandbox_path(base_dir, name + '.dat')
    file_path.write_bytes(written)
    read = file_path.read_bytes()
    assert read == written


# noinspection SpellCheckingInspection
@pytest.mark.parametrize(
    'data',
    [
        ('latin', 'ascii', u"Two wrongs don't make a right.\nThe pen is mightier than the sword."),
        ('latin', 'utf8', u"Alla sätt är bra utom de dåliga.\nGräv där du står."),
        ('cyrillic', 'utf8', u"А дело бывало — и коза волка съедала.\nАзбука — к мудрости ступенька."),
        ('cyrillic', 'utf16', u"Загляне сонца i ў наша ваконца.\nКаб свiнне poгi - нiкому б не было дарогi."),
        ('pinyin', 'utf8', u"防人之心不可無。\n福無重至,禍不單行。"),
        ('pinyin', 'utf16', u"[与其]临渊羡鱼，不如退而结网。\n君子之心不胜其小，而气量涵盖一世。"),
        ],
    ids=lambda data: '{0}-{1}'.format(*data))
def test_write_read_text(data, base_dir):
    name, encoding, written = data
    file_path = prepare_sandbox_path(base_dir, name + '.txt')
    file_path.write_text(written, encoding=encoding)
    read = file_path.read_text(encoding=encoding)
    assert read == written


def test_rmtree_mkdir(base_dir):
    path = base_dir / 'tree'
    path.parent.mkdir(exist_ok=True, parents=True)
    path.rmtree(ignore_errors=True)
    with pytest.raises(Exception):
        path.rmtree(ignore_errors=False)
    path.mkdir(exist_ok=False)
    grand_child_path = path / 'level_1_dir' / 'level_2_dir'
    with pytest.raises(Exception):
        grand_child_path.mkdir(parents=False)
    grand_child_path.mkdir(parents=True)
    grand_child_path.with_name('level_2_file').write_bytes(b'F\0\0D')
    path.rmtree()
    assert not path.exists()


def test_upload_download(base_dir):
    path = prepare_sandbox_path(base_dir, 'uploaded.dat')
    local_source_path = Path('/tmp/func_tests/paths_test_sandbox/to_upload.dat')  # Not dependent on base_dir!
    if not local_source_path.exists():
        local_source_path.write_bytes(os.urandom(1000 * 1000))
    path.upload(local_source_path)
    local_destination_path = base_dir / 'download.dat'
    path.download(local_destination_path)
    assert filecmp.cmp(str(local_destination_path), str(local_source_path))


def test_upload_destination_is_dir(base_dir):
    destination_path = base_dir / 'dir_preventing_upload'
    destination_path.rmtree()
    destination_path.mkdir()
    local_source_path = Path('/tmp/func_tests/paths_test_sandbox/to_upload.dat')  # Not dependent on base_dir!
    with pytest.raises(Exception):
        destination_path.upload(local_source_path)
