
import cssutils
import logging
import os
from inlinestyler.utils import inline_css

import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try 'pip install inlinestyler==0.2.1' and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

containter_text = open('container.mustache.template').read()
containter_text = containter_text.replace('{{>style_css}}', open('style.css').read())

for file in os.listdir("."):
    if file.endswith(".mustache.body"):
        f = file.replace('.mustache.body', '')
        newf = open(f + '.mustache', 'w')
        text = containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
        print >> newf, inline_css(text).encode('utf-8','replace')
