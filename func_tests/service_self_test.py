from test_utils.service import STATUS_RE


def test_re():
    assert STATUS_RE.match('hanwha-mediaserver start/running, process 2145\n').group('status') == 'start/running'
    assert STATUS_RE.match('hanwha-mediaserver start/running, process 2145\n').group('pid') == '2145'
    assert STATUS_RE.match('hanwha-mediaserver start/running, process 2145\n').group('name') == 'hanwha-mediaserver'
    assert STATUS_RE.match('hanwha-mediaserver stop/waiting\n').group('status') == 'stop/waiting'
