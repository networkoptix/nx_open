import re

from collections import namedtuple

from pathlib2 import PurePosixPath

Version = namedtuple('Version', ['major', 'minor', 'fix', 'build'])
Customization = namedtuple('Customization', ['company_name', 'service_name', 'installation_subdir'])
BuildInfo = namedtuple('BuildInfo', [
    'version', 'beta', 'proto_version', 'change_set',
    'customization', 'brand', 'cloud_host', 'cloud_group',
    'arch', 'platform', 'platform_modification'])

_FIRST_CAP_RE = re.compile('([A-Za-z])([A-Z][a-z]+)')
_ALL_CAP_RE = re.compile('([a-z0-9])([A-Z])')


def _camel_case_to_underscore(name):
    name = _FIRST_CAP_RE.sub(r'\1_\2', name)
    name = _ALL_CAP_RE.sub(r'\1_\2', name)
    name = name.lower()
    return name


def version_from_str(version_as_str):
    parts_as_str = version_as_str.split('.', 3)
    parts = [int(part) for part in parts_as_str]
    return Version(*parts)


def customization_from_company_name(company_name):
    return Customization(company_name, company_name + '-mediaserver', PurePosixPath(company_name, 'mediaserver'))


def customizations_from_paths(paths, installation_root):
    for path in paths:
        if path.name == 'mediaserver' and path.parent.parent == installation_root:
            yield customization_from_company_name(path.parent.name)


def build_info_from_text(text):
    version = None
    parts = {}
    for line in text.splitlines(False):
        name, value = line.split('=', 1)
        if name == 'version':
            version = version_from_str(value)
        else:
            parts[_camel_case_to_underscore(name)] = value
    return BuildInfo(version=version, **parts)
