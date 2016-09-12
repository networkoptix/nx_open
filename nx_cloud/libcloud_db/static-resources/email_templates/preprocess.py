import cssutils
import logging
from inlinestyler.utils import inline_css

cssutils.log.setLevel(logging.ERROR)

containter_text = open('container.mustache.template').read()
containter_text = containter_text.replace('{{>style_css}}', open('style_css.mustache.body').read())

for f in ('account_confirmation',):
    newf = open(f + '.mustache', 'w')
    text = containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
    print >> newf, inline_css(text).encode('utf-8','replace')
