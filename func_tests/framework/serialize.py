from collections import OrderedDict

import yaml
import yaml.emitter
import yaml.representer
import yaml.resolver
import yaml.serializer
from yaml.constructor import ConstructorError


class _Loader(yaml.Loader):
    def construct_mapping(self, node, deep=False):
        if not isinstance(node, yaml.MappingNode):
            raise ConstructorError(
                None, None,
                "expected a mapping node, but found %s" % node.id, node.start_mark)
        mapping = OrderedDict()
        for key_node, value_node in node.value:
            key = self.construct_object(key_node, deep=deep)
            try:
                hash(key)
            except TypeError as exc:
                raise ConstructorError(
                    "while constructing a mapping", node.start_mark,
                    "found unacceptable key (%s)" % exc, key_node.start_mark)
            value = self.construct_object(value_node, deep=deep)
            mapping[key] = value
        return mapping


_Loader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,
    _Loader.construct_mapping,
    )


def load(raw):
    return yaml.load(raw.encode())


class _Representer(yaml.representer.Representer):
    def represent_none(self, data):
        return self.represent_scalar(u'tag:yaml.org,2002:null', u'')


_Representer.add_representer(type(None), _Representer.represent_none)
_Representer.add_representer(OrderedDict, _Representer.represent_dict)


class _Dumper(yaml.emitter.Emitter, yaml.serializer.Serializer, _Representer, yaml.resolver.Resolver):
    def __init__(
            self, stream, default_style=None, default_flow_style=None,
            canonical=None, indent=None, width=None,
            allow_unicode=None, line_break=None,
            encoding=None, explicit_start=None, explicit_end=None,
            version=None, tags=None):
        yaml.emitter.Emitter.__init__(
            self, stream, canonical=canonical,
            indent=indent, width=width,
            allow_unicode=allow_unicode, line_break=line_break)
        yaml.serializer.Serializer.__init__(
            self, encoding=encoding,
            explicit_start=explicit_start, explicit_end=explicit_end,
            version=version, tags=tags)
        _Representer.__init__(
            self, default_style=default_style,
            default_flow_style=default_flow_style)
        yaml.resolver.Resolver.__init__(self)


def dump(data):
    return yaml.dump(data, Dumper=_Dumper, default_flow_style=False).decode()
