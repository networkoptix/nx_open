def test_artifacts(artifacts_dir):
    dirs = [
        artifacts_dir,
        artifacts_dir / 'sub',
        artifacts_dir / 's u b',
        ]
    for dir in dirs:
        dir.mkdir(exist_ok=True)
        dir.joinpath('plain.txt').write_text(u'test')
        dir.joinpath('log.log').write_text(u'test')
        dir.joinpath('no_suffix').write_text(u'test')
        dir.joinpath('ends_with_dot.').write_text(u'test')
        dir.joinpath('s p a c e s.txt').write_text(u'test')
        dir.joinpath('weird.suffix').write_text(u'test')
        dir.joinpath('json.json').write_text(u'{"uh":"oh"}')
        dir.joinpath('yaml.yaml').write_text(u'uh: oh')
