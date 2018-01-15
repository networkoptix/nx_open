import os.path

import jinja2

TEST_UTILS_DIR = os.path.abspath(os.path.dirname(__file__))


class TemplateRenderer(object):

    def __init__(self):
        templates_dir = TEST_UTILS_DIR
        loader = jinja2.FileSystemLoader(templates_dir)
        self._env = jinja2.Environment(loader=loader)

    def render(self, template_path, **kw):
        template = self._env.get_template(template_path)
        return template.render(**kw)
