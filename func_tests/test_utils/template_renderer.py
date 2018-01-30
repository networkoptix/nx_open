import jinja2
from pathlib2 import Path

TEST_UTILS_DIR = Path(__file__).resolve().parent


class TemplateRenderer(object):

    def __init__(self):
        templates_dir = TEST_UTILS_DIR
        loader = jinja2.FileSystemLoader(str(templates_dir))
        self._env = jinja2.Environment(loader=loader)

    def render(self, template_path, **kw):
        template = self._env.get_template(str(template_path))
        return template.render(**kw)
