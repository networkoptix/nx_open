import datetime
import logging
from collections import OrderedDict
from pprint import pformat
from xml.dom import minidom

import dateutil
import pytz
import xmltodict
from pylru import lrudecorator
from typing import Mapping, Union
from winrm.exceptions import WinRMError, WinRMTransportError

_logger = logging.getLogger(__name__)


def _pretty_format_xml(text):
    dom = minidom.parseString(text)
    pretty_text = dom.toprettyxml(indent=' '*2)
    return pretty_text


# Explanation: https://docs.microsoft.com/en-us/windows/desktop/WmiSdk/wmi-error-constants.
# More: https://docs.microsoft.com/en-us/windows/desktop/adsi/win32-error-codes.
_win32_error_codes = {
    # Errors originating in the core operating system.
    # Remove 0x8007, lookup for last 4 hex digits.
    # See: https://docs.microsoft.com/en-us/windows/desktop/debug/system-error-codes.
    0x80071392: 'ERROR_OBJECT_ALREADY_EXISTS',
    # WBEM Errors.
    # See: https://docs.microsoft.com/en-us/windows/desktop/WmiSdk/wmi-error-constants.
    0x80041008: 'WBEM_E_INVALID_PARAMETER',
    }


class Wmi(object):
    def __init__(self, protocol):
        self.protocol = protocol

    def __eq__(self, other):
        if not isinstance(other, Wmi):
            return NotImplemented
        return other.protocol is self.protocol

    def act(self, class_uri, action, body, selectors, timeout_sec=None):
        xml_namespaces = {
            # Namespaces from pywinrm's Protocol.
            # TODO: Use aliases from WS-Management spec.
            # See: https://www.dmtf.org/sites/default/files/standards/documents/DSP0226_1.0.0.pdf, Table A-1.
            # Currently aliases are coupled from those from Protocol.
            'http://www.w3.org/XML/1998/namespace': 'xml',
            'http://www.w3.org/2003/05/soap-envelope': 'env',
            'http://schemas.xmlsoap.org/ws/2004/09/enumeration': 'n',
            'http://www.w3.org/2001/XMLSchema-instance': 'xsi',
            'http://www.w3.org/2001/XMLSchema': 'xs',
            'http://schemas.dmtf.org/wbem/wscim/1/common': 'cim',
            'http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd': 'w',
            'http://schemas.xmlsoap.org/ws/2004/09/transfer': 't',
            'http://schemas.xmlsoap.org/ws/2004/08/addressing': 'a',
            self.cls('Win32_FolderRedirectionHealth').uri: 'folder',
            # Commonly encountered in responses.
            class_uri: None,  # Desired class namespace is default.
            }

        def _substitute_namespace_in_type_attribute(path, key, data):
            """Process namespaces in type attr value."""
            if key != '@' + xml_namespaces['http://www.w3.org/2001/XMLSchema-instance'] + ':type':
                return key, data
            element_namespace_aliases = path[-1][1].get('xmlns', {})
            if element_namespace_aliases is None:
                return key, data
            old_namespace_alias, bare_data = data.split(':')
            new_namespace_alias = xml_namespaces[element_namespace_aliases[old_namespace_alias]]
            if new_namespace_alias is None:
                return key, bare_data
            return key, new_namespace_alias + ':' + bare_data

        # noinspection PyProtectedMember
        rq = {
            'env:Envelope': self.protocol._get_soap_header(resource_uri=class_uri, action=action)}
        rq['env:Envelope'].setdefault('env:Body', body)
        rq['env:Envelope']['env:Header']['w:SelectorSet'] = selectors.raw
            rq['env:Envelope']['env:Header']['w:OperationTimeout'] = 'PT{}S'.format(timeout_sec)
        try:
            request_xml = xmltodict.unparse(rq)
            _logger.debug("Request XML:\n%s", _pretty_format_xml(request_xml))
            response = self.protocol.send_message(request_xml)
            _logger.debug("Response XML:\n%s", _pretty_format_xml(response))
        except WinRMTransportError as e:
            _logger.exception("XML:\n%s", e.response_text)
            raise
        except WinRMError as e:
            _logger.debug("Failed: %s", e)
            raise

        response_dict = xmltodict.parse(
            response,
            dict_constructor=dict,  # Order is meaningless.
            process_namespaces=True, namespaces=xml_namespaces,  # Force namespace aliases.
            force_list=['w:Item', 'w:Selector'],
            postprocessor=_substitute_namespace_in_type_attribute,
            )
        return response_dict['env:Envelope']['env:Body']

    def cls(
            self,
            name,
            namespace='root/cimv2',
            root_uri='http://schemas.microsoft.com/wbem/wsman/1/wmi'):
        # TODO: Check URIs case-insensitively.
        normalized_namespace = namespace.replace('\\', '/').lower()
        uri = root_uri + '/' + normalized_namespace + '/' + name
        return _Class(self, uri)


class _Class(object):
    """WMI class on specific machine. Create or enumerate objects with this class. To work with
    one specific object, use _Reference class. Use _Class.reference method to get reference to
    object of this class."""

    def __init__(self, wmi, uri):
        self.wmi = wmi
        self.uri = uri
        self.name = uri.rsplit('/', 1)[-1]

    def __repr__(self):
        return '_Class({!r}, {!r})'.format(self.wmi, self.uri)

    def __eq__(self, other):
        if not isinstance(other, _Class):
            return NotImplemented
        return other.wmi == self.wmi and other.uri == self.uri

    def create(self, properties_dict):
        _logger.info("Create %r: %r", self, properties_dict)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Create'
        body = {self.name: _prepare_data(properties_dict)}
        body[self.name]['@xmlns'] = self.uri
        outcome = self.wmi.act(self.uri, action_url, body, _SelectorSet.empty())
        reference_parameters = outcome[u't:ResourceCreated'][u'a:ReferenceParameters']
        assert reference_parameters[u'w:ResourceURI'] == self.uri
        selector_set = reference_parameters[u'w:SelectorSet']
        return selector_set

    def enumerate(self, selectors_dict, max_elements=32000):
        _logger.info("Enumerate %s where %r", self, selectors_dict)
        selectors = _SelectorSet.from_dict(selectors_dict)
        enumeration_context = [None]
        enumeration_is_ended = [False]

        def _start():
            _logger.debug("Start enumerating %s where %r", self, selectors)
            assert not enumeration_is_ended[0]
            action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate'
            body = {
                'n:Enumerate': {
                    'w:OptimizeEnumeration': None,
                    'w:MaxElements': str(max_elements),
                    # Set mode as `EnumerateObjectAndEPR` to get selector set for reference and
                    # be able to invoke methods on returned object.
                    # See: https://www.dmtf.org/sites/default/files/standards/documents/DSP0226_1.0.0.pdf, 8.7.
                    'w:EnumerationMode': 'EnumerateObjectAndEPR',
                    }
                }
            response = self.wmi.act(self.uri, action, body, selectors)
            enumeration_context[0] = response['n:EnumerateResponse']['n:EnumerationContext']
            enumeration_is_ended[0] = 'w:EndOfSequence' in response['n:EnumerateResponse']
            items = response['n:EnumerateResponse']['w:Items']['w:Item']
            return _pick_objects(items)

        def _pull():
            _logger.debug("Continue enumerating %s where %r", self, selectors)
            assert enumeration_context[0] is not None
            assert not enumeration_is_ended[0]
            action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Pull'
            body = {
                'n:Pull': {
                    'n:EnumerationContext': enumeration_context[0],
                    'n:MaxElements': str(max_elements),
                    }
                }
            response = self.wmi.act(self.uri, action, body, selectors)
            enumeration_is_ended[0] = 'n:EndOfSequence' in response['n:PullResponse']
            enumeration_context[0] = None if enumeration_is_ended[0] else response['n:PullResponse']['n:EnumerationContext']
            items = response['n:PullResponse']['n:Items']['w:Item']
            return _pick_objects(items)

        def _pick_objects(item_list):
            # `EnumerationMode` must be `EnumerateObjectAndEPR` to have both data and selectors.
            for item in item_list:
                data = item[self.name]
                ref = _Reference.from_raw(self.wmi, item['a:EndpointReference'])
                assert ref.wmi_class == self
                obj = _Object(ref, data)
                _logger.info('\tObject: %r', obj)
                yield obj

        for item in _start():
            yield item
        while not enumeration_is_ended[0]:
            for item in _pull():
                yield item

    def reference(self, selectors):
        """Refer to objects of this class via this method."""
        return _Reference(self, _SelectorSet.from_dict(selectors))

    def static(self):
        """Use to call "static" methods. E.g. for StdRegProv class."""
        return self.reference({})

    def singleton(self):
        """Use to get singleton. E.g., Win32_OperatingSystem."""
        return self.static().get()


class _SelectorSet(object):
    def __init__(self, raw, as_dict):
        self.raw = raw
        self.as_dict = as_dict

    def __repr__(self):
        return '_SelectorSet.from_dict({!r})'.format(self.as_dict)

    def __eq__(self, other):
        if not isinstance(other, _SelectorSet):
            return NotImplemented
        return other.as_dict == self.as_dict

    @classmethod
    def from_dict(cls, as_dict):
        raw = {
            'w:Selector': [
                {'@Name': selector_name, '#text': selector_value}
                if selector_value is not None else
                {'@Name': selector_name}
                for selector_name, selector_value in as_dict.items()
                ]
            }
        return cls(raw, as_dict)

    @classmethod
    def from_raw(cls, raw):
        as_dict = {s['@Name']: s.get('#text') for s in raw['w:Selector']}
        return cls(raw, as_dict)

    @classmethod
    def empty(cls):
        return cls.from_dict({})


def _prepare_data(data):
    if data is None:
        return {'@xsi:nil': 'true'}
    if data == '':
        return None
    if isinstance(data, datetime.datetime):
        return {'cim:Datetime': data.astimezone(pytz.UTC).strftime('%Y-%m-%dT%H:%M:%S.%fZ')}
    if isinstance(data, (dict, OrderedDict)):
        return {key: _prepare_data(data[key]) for key in data}
    return data


def _parse_data(data):
    if data is None:
        return ''
    if data == {'@xsi:nil': 'true'}:
        return None
    if isinstance(data, (dict, OrderedDict)):
        if 'cim:Datetime' in data:
            return dateutil.parser.parse(data['cim:Datetime'])
        return {key: _parse_data(data[key]) for key in data}
    return data


class _Reference(object):
    """Reference to single object. Get single object and invoke methods here."""

    def __init__(self, wmi_class, selectors):
        self.wmi_class = wmi_class
        self.selectors = selectors

    def __repr__(self):
        return '_Reference({!r}, {!r})'.format(self.wmi_class, self.selectors)

    def __eq__(self, other):
        if not isinstance(other, _Reference):
            return NotImplemented
        return other.wmi_class == self.wmi_class and other.selectors == self.selectors

    def __getitem__(self, item):
        return self.selectors.as_dict[item]

    @classmethod
    def from_raw(cls, wmi, raw):
        wmi_class = _Class(wmi, raw['a:ReferenceParameters']['w:ResourceURI'])
        # `SelectorSet` isn't always present, e.g. with `Win32_OperatingSystem`.
        if 'w:SelectorSet' in raw['a:ReferenceParameters']:
            selector_set = _SelectorSet.from_raw(raw['a:ReferenceParameters']['w:SelectorSet'])
        else:
            selector_set = _SelectorSet.empty()
        return cls(wmi_class, selector_set)

    def get(self):
        _logger.info("Get %s where %r", self.wmi_class, self.selectors)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Get'
        outcome = self.wmi_class.wmi.act(self.wmi_class.uri, action_url, {}, self.selectors)
        instance = outcome[self.wmi_class.name]
        return _Object(self, instance)

    def put(self, new_properties_dict):
        _logger.info("Put %s where %r: %r", self.wmi_class, self.selectors, new_properties_dict)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Put'
        body = {self.wmi_class.name: _prepare_data(new_properties_dict)}
        body[self.wmi_class.name]['@xmlns'] = self.wmi_class.uri
        outcome = self.wmi_class.wmi(self.wmi_class.uri, action_url, body, self.selectors)
        instance = outcome[self.wmi_class.name]
        return _Object(self, instance)

    def invoke_method(self, method_name, params, timeout_sec=None):
        _logger.info("Invoke %s.%s(%r) where %r", self.wmi_class, method_name, params, self.selectors)
        action_uri = self.wmi_class.uri + '/' + method_name
        method_input = {'p:' + param_name: param_value for param_name, param_value in _prepare_data(params).items()}
        method_input['@xmlns:p'] = self.wmi_class.uri
        method_input['@xmlns:cim'] = 'http://schemas.dmtf.org/wbem/wscim/1/common'  # For `cWMIim:Datetime`.
        method_input['@xmlns:xsi'] = 'http://www.w3.org/2001/XMLSchema-instance'  # For `@xsi:nil`.
        body = {method_name + '_INPUT': method_input}
        response = self.wmi_class.wmi.act(self.wmi_class.uri, action_uri, body, self.selectors, timeout_sec=timeout_sec)
        method_output = _parse_data(response[method_name + '_OUTPUT'])
        if method_output[u'ReturnValue'] != u'0':
            exception_cls = WmiInvokeFailed.specific_cls(int(method_output[u'ReturnValue']))
            raise exception_cls(self, method_name, params, method_output)
        return method_output


class _Object(object):
    def __init__(self, ref, data):  # type: (_Reference, Mapping[str, Union[str, Mapping]]) -> ...
        self.ref = ref
        self._data = {}
        for key, value in _parse_data(data).items():
            if key == '@xmlns' or key == '@xsi:type':
                continue
            if isinstance(value, (dict, OrderedDict)):
                if 'a:ReferenceParameters' in value:
                    self._data[key] = _Reference.from_raw(ref.wmi_class.wmi, value)
                    continue
            self._data[key] = value

    def __repr__(self):
        return '_Object({!r}, {!r})'.format(self.ref, self._data)

    def __getitem__(self, item):
        return self._data[item]

    def put(self, new_properties_dict):  # type: (Mapping[str, str]) -> _Object
        return self.ref.put(new_properties_dict)

    def invoke_method(self, method_name, params, timeout_sec=None):
        return self.ref.invoke_method(method_name, params, timeout_sec=timeout_sec)


class WmiInvokeFailed(Exception):
    @classmethod
    @lrudecorator(100)
    def specific_cls(cls, rv):
        class WmiInvokeFailed(cls):
            return_value = rv

            def __init__(self, wmi_reference, method_name, params, method_output):
                super(WmiInvokeFailed, self).__init__(
                    'Non-zero return value 0x{:X} ({}) of {!s}.{!s}({!r}) where {!r}:\n{!s}'.format(
                        self.return_value,
                        _win32_error_codes.get(self.return_value, 'unknown'),
                        wmi_reference.wmi_class.name,
                        method_name,
                        params,
                        wmi_reference.selectors,
                        pformat(method_output),
                        ))
                self.wmi_reference = wmi_reference
                self.method_name = method_name
                self.arguments = params
                self.output = method_output

        return WmiInvokeFailed


def find_by_selector_set(selector_set, items):
    """Selector is field-value pair to find value. Selector set is used as foreign key when objects
    are referred in WSMan response. Sometimes fields in its selector set are undocumented but it's
    OK to used them: PowerShell does this way.
    """
    for item in items:
        for selector in selector_set['w:Selector']:
            if item[selector['@Name']] != selector['#text']:
                break  # One of selectors is not met. Skip item. Break from loop over criteria.
        else:
            yield item  # All selectors are fulfilled.
