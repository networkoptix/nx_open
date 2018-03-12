import re

from collections import namedtuple

Version = namedtuple('Version', ['major', 'minor', 'fix', 'build'])
Customization = namedtuple('Customization', ['name', 'company', 'service', 'installation_subdir'])
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


def _version_from_str(version_as_str):
    parts_as_str = version_as_str.split('.', 3)
    parts = [int(part) for part in parts_as_str]
    return Version(*parts)


def customizations_from_paths(paths, installation_root):
    for path in paths:
        if path.name == 'mediaserver' and path.parent.parent == installation_root:
            company = path.parent.name
            yield Customization(
                name=('default' if company == 'networkoptix' else company),
                company=company,
                service=company + '-mediaserver',
                installation_subdir=path.relative_to(installation_root))


def build_info_from_text(text):
    version = None
    parts = {}
    for line in text.splitlines(False):
        name, value = line.split('=', 1)
        if name == 'version':
            version = _version_from_str(value)
        else:
            parts[_camel_case_to_underscore(name)] = value
    return BuildInfo(version=version, **parts)
