
import cssutils
import logging
import os
from inlinestyler.utils import inline_css

import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try 'pip install inlinestyler==0.2.1' and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

containter_text = open('container.mustache.template').read()
containter_text = containter_text.replace('{{>style_css}}', open('style.css').read())

containter_text_plain = open('container_plain.mustache.template').read()

for file in os.listdir("."):
    if file.endswith("_plain.mustache.body"):
        f = file.replace('.mustache.body', '')
        text = containter_text_plain.replace('{{>body}}', open(f + '.mustache.body').read())
        with open(f + '.mustache', 'wb') as newf:
            newf.write(text + '\n')
    elif file.endswith(".mustache.body"):
        f = file.replace('.mustache.body', '')
        text = containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
        with open(f + '.mustache', 'wb') as newf:
            newf.write(inline_css(text).encode('utf-8','replace') + '\n')
