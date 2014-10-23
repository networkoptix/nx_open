import cssutils
import logging
from inlinestyler.utils import inline_css

cssutils.log.setLevel(logging.ERROR)

containter_text = open('container.mustache.template').read()
containter_text = containter_text.replace('{{>style_css}}', open('style_css.mustache.body').read())

for f in ('camera_input', 'camera_ip_conflict', 'camera_motion', 'mediaserver_conflict', 'mediaserver_failure', 'mediaserver_started', 'storage_failure', 'license_issue'):
    newf = open(f + '.mustache', 'w')
    text = containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
    print >> newf, inline_css(text).encode('utf-8','replace')

multi_res_containter_text = open('multi_res_container.mustache.template').read()
multi_res_containter_text = multi_res_containter_text.replace('{{>style_css}}', open('style_css.mustache.body').read())

for f in ('camera_disconnect', 'network_issue'):
    newf = open(f + '.mustache', 'w')
    text = multi_res_containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
    print >> newf, inline_css(text).encode('utf-8','replace')

    