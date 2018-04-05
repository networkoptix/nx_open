from collections import namedtuple

import yaml
from pathlib2 import Path

Layout = namedtuple('Layout', ['networks', 'mergers'])


def get_layout(file_name):
    path = LAYOUTS_DIR / file_name
    contents = path.read_text()
    data = yaml.load(contents)
    layout = Layout(
        networks=data['networks'],
        mergers=data.get('mergers') or {})
    return layout
