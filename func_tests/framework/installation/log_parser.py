import datetime
import logging
import re

_ts_format = '%Y-%m-%d %H:%M:%S.%f'
_split_re = re.compile(
    r'''
        ^ (?P<ts> \d{4}-\d{2}-\d{2} \s+ \d{2}:\d{2}:\d{2}.\d{3} )
        \x20+ (?P<thread>[0-9a-fA-F]{1,6})
        \x20+ (?P<level>ALWAYS|ERROR|WARNING|INFO|DEBUG|VERBOSE)
        \x20 (?P<tag> .*?)
        :\x20
        ''',
    re.MULTILINE | re.VERBOSE)

_level_map = {
    'ALWAYS': logging.CRITICAL + 5,
    'ERROR': logging.ERROR,
    'WARNING': logging.WARNING,
    'INFO': logging.INFO,
    'DEBUG': logging.DEBUG,
    'DEBUG2': logging.DEBUG - 5,
    'VERBOSE': logging.DEBUG - 5,
    }


def _record(prev_match, message):
    return {
        'ts': datetime.datetime.strptime(prev_match.group('ts'), _ts_format).isoformat(),
        'message': message,  # -1 for newline.
        'name': prev_match.group('tag'),
        'levelname': prev_match.group('level'),
        'levelno': _level_map[prev_match.group('level')],
        'thread': prev_match.group('thread'),  # Hex on Windows, decimal on Linux.
        }


def parse_mediaserver_logs(log_bytes):
    log_text = log_bytes.decode('utf-8-sig')
    matches_iter = _split_re.finditer(log_text)
    prev_match = next(matches_iter)
    assert prev_match.start() == 0
    for next_match in matches_iter:
        message = log_text[prev_match.end():next_match.start() - 1]
        yield _record(prev_match, message)
        prev_match = next_match
    yield _record(prev_match, log_text[prev_match.end():-1])


def parse_mediaserver_logs_test():
    sample = (
        b'\xef\xbb\xbf2018-10-13 19:23:26.753   6090   DEBUG MediaServerProcess: Creating ...\n'
        b'2018-10-13 19:23:26.753   6090   DEBUG MediaServerProcess: Storage new candidates:\n'
        b'                url: /opt/networkoptix/mediaserver/var/data, ...\n'
        b'2018-10-13 19:23:26.756   6090   DEBUG MediaServerProcess: QnStorageResourceList ...\n'
        b'2018-10-14 11:16:43.819  13523   DEBUG : Applying SQL update :/mserver_updates/01...\n'
        b'2018-10-14 11:16:43.819  13523   DEBUG : Applying SQL update :/mserver_updates/02...\n'
        b'\n')
    result = list(parse_mediaserver_logs(sample))
    assert len(result) == 5
    assert result[0]['levelname'] == 'DEBUG'
    assert result[0]['name'] == 'MediaServerProcess'
    assert result[0]['message'] == 'Creating ...'
    assert '\n' in result[1]['message']


if __name__ == '__main__':
    parse_mediaserver_logs_test()
