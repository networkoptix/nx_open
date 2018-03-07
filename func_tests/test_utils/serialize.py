import yaml
import yaml.emitter
import yaml.representer
import yaml.resolver
import yaml.serializer


def load(raw):
    return yaml.load(raw)


class _Representer(yaml.representer.Representer):
    def represent_none(self, data):
        return self.represent_scalar(u'tag:yaml.org,2002:null', u'')


_Representer.add_representer(type(None), _Representer.represent_none)


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
    return yaml.dump(data, Dumper=_Dumper, default_flow_style=False)
