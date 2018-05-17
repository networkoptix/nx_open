import logging
from subprocess import check_output

import pytest

from framework.os_access.posix_shell_utils import sh_command_to_script

_logger = logging.getLogger(__name__)


def _tricky_string_list_to_test_id(args):
    _replacements = {
        '{': '{leftbrace}',
        '}': '{rightbrace}',
        '_': '{underscore}',
        '"': '{doublequote}',
        "'": '{quote}',
        '!': '{exclamation}',
        ' ': '{space}',
        '\n': '{newline}',
        '\t': '{tab}',
        '\\': '{backslash}',
        '$': '{dollar}',
        '`': '{backtick}',
        }
    return '__'.join((
        ''.join(_replacements.get(char, char) for char in arg)
        for arg in args
        ))


_tricky_strings = [
    ["'"], ['"'], ['{'], ['}'], ['!'], ['\n'], ['\t'], ['\\'], ['['], [']'], ['`'], ['$'],
    ['arg1word1 arg1word2', 'arg2'], ['arg1', 'arg2word1 arg2word2'],
    [' within spaces '], ['double  space'],
    [" \\' \\' "], [" \\' ", " \\' "],
    ["a 'b c' d"], ["a 'b ", " c' d"], ["a 'b", "c' d"],
    ['a "b c" d'], ['a "b ', ' c" d'], ['a "b', 'c" d'],
    ['QWE=123', '$QWE'], ['QWE=123', '${QWE}'], ['QWE=123 ${QWE}'],
    ]
_string_begin = '[['
_string_end = ']]'
assert not any(_string_begin in string or _string_end in string for string in _tricky_strings)


@pytest.mark.parametrize('strings', _tricky_strings, ids=_tricky_string_list_to_test_id)
@pytest.mark.parametrize('indirection_level', [0, 1, 2], ids='indirection{}'.format)
def test_basics(strings, indirection_level):
    script = 'for string in {strings}; do echo -n {begin}"$string"{end}; done'.format(
        strings=sh_command_to_script(strings), begin=_string_begin, end=_string_end)
    for _ in range(indirection_level):
        script = sh_command_to_script(['sh', '-c', script])
    _logger.debug('Script:\n%s', script)
    output_with_strings = check_output(script, shell=True)
    assert output_with_strings.startswith(_string_begin) and output_with_strings.endswith(_string_end)
    strings_parsed = output_with_strings[len(_string_begin):-len(_string_end)].split(_string_end + _string_begin)
    assert strings_parsed == strings
