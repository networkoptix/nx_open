
import cssutils
import logging
from inlinestyler.utils import inline_css

import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try 'pip install inlinestyler==0.2.1' and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

containter_text = open('container.mustache.template').read()
containter_text = containter_text.replace('{{>style_css}}', open('style.css').read())

for f in ('camera_input', 'camera_ip_conflict', 'camera_motion', 'mediaserver_conflict', 'generic_event',
        'mediaserver_failure', 'mediaserver_started', 'storage_failure', 'license_issue',
        'camera_disconnect', 'network_issue'):
    newf = open(f + '.mustache', 'w')
    text = containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
    print >> newf, inline_css(text).encode('utf-8','replace')
