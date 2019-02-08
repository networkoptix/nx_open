import cssutils
import logging
import codecs
import os
import sys
from inlinestyler.utils import inline_css

import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try 'pip install inlinestyler==0.2.1' and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

def handle_file(file, containter_text, containter_text_plain):
    if file.endswith("_plain.mustache.body"):
        f = file.replace('.mustache.body', '')
        text = containter_text_plain.replace('{{>body}}', open(f + '.mustache.body').read())
        with open(f + '.mustache', 'w') as newf:
            newf.write(text)
    elif file.endswith(".mustache.body"):
        f = file.replace('.mustache.body', '')
        text = containter_text.replace('{{>body}}', open(f + '.mustache.body').read())
        with codecs.open(f + '.mustache', "w", "utf-8") as newf:
            newf.write(inline_css(text))

def main():
    containter_text = open('container.mustache.template').read()
    containter_text = containter_text.replace('{{>style_css}}', open('style.css').read())

    containter_text_plain = open('container_plain.mustache.template').read()

    files_to_process = sys.argv[1:] if len(sys.argv) > 1 else os.listdir(".")
    for file in files_to_process:
        handle_file(file, containter_text, containter_text_plain)

if __name__ == "__main__":
    main()
