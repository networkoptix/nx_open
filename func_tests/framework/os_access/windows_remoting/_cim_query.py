import logging
from pprint import pformat
from xml.dom import minidom

import xmltodict
from winrm.exceptions import WinRMError, WinRMTransportError

_logger = logging.getLogger(__name__)


def _pretty_format_xml(text):
    dom = minidom.parseString(text)
    pretty_text = dom.toprettyxml(indent=' '*2)
    return pretty_text


# Some error codes of interest to us
# https://docs.microsoft.com/en-us/windows/desktop/WmiSdk/wmi-error-constants
# https://docs.microsoft.com/en-us/windows/desktop/debug/system-error-codes
# https://docs.microsoft.com/en-us/windows/desktop/adsi/win32-error-codes
_win32_error_codes = {
    0x80071392: 'ERROR_OBJECT_ALREADY_EXISTS',
    }

class CIMClass(object):
    default_namespace = 'root/cimv2'
    default_root_uri = 'http://schemas.microsoft.com/wbem/wsman/1/wmi'

    def __init__(self, name, namespace=default_namespace, root_uri=default_root_uri):
        self.name = name
        self.original_namespace = namespace
        self.normalized_namespace = namespace.replace('\\', '/').lower()
        self.root_uri = root_uri
        self.uri = root_uri + '/' + self.normalized_namespace + '/' + name

    def __repr__(self):
        if self.root_uri == self.default_root_uri:
            if self.normalized_namespace == self.default_namespace:
                return 'CimClass({!r})'.format(self.name)
            return 'CimClass({!r}, namespace={!r})'.format(self.name, self.original_namespace)
        return 'CimClass({!r}, namespace={!r}, root_uri={!r})'.format(self.name, self.original_namespace, self.root_uri)

    def method_uri(self, method_name):
        return self.uri + '/' + method_name


class CIMQuery(object):
    def __init__(self, protocol, cim_class, selectors):
        self.protocol = protocol
        self.cim_class = cim_class
        self.selectors = selectors

    def _perform_action(self, action, body, timeout_sec=None):
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
            CIMClass('Win32_FolderRedirectionHealth').uri: 'folder',
            # Commonly encountered in responses.
            self.cim_class.uri: None,  # Desired class namespace is default.
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
        rq = {'env:Envelope': self.protocol._get_soap_header(resource_uri=self.cim_class.uri, action=action)}
        rq['env:Envelope'].setdefault('env:Body', body)
        rq['env:Envelope']['env:Header']['w:SelectorSet'] = {
            'w:Selector': [
                {'@Name': selector_name, '#text': selector_value}
                for selector_name, selector_value in self.selectors.items()
                ]
            }
        if timeout_sec is not None:
            rq['env:Envelope']['w:OperationTimeout'] = 'PT{}S'.format(timeout_sec)
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

    def enumerate(self, max_elements=32000):
        _logger.info("Enumerate %s where %r", self.cim_class, self.selectors)
        enumeration_context = [None]
        enumeration_is_ended = [False]

        def _start():
            _logger.debug("Start enumerating %s where %r", self.cim_class, self.selectors)
            assert not enumeration_is_ended[0]
            action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate'
            body = {
                'n:Enumerate': {
                    'w:OptimizeEnumeration': None,
                    'w:MaxElements': str(max_elements),
                    # See: https://www.dmtf.org/sites/default/files/standards/documents/DSP0226_1.0.0.pdf, 8.7.
                    'w:EnumerationMode': 'EnumerateObjectAndEPR',
                    }
                }
            response = self._perform_action(action, body)
            enumeration_context[0] = response['n:EnumerateResponse']['n:EnumerationContext']
            enumeration_is_ended[0] = 'w:EndOfSequence' in response['n:EnumerateResponse']
            items = response['n:EnumerateResponse']['w:Items']['w:Item']
            return _pick_objects(items)

        def _pull():
            _logger.debug("Continue enumerating %s where %r", self.cim_class, self.selectors)
            assert enumeration_context[0] is not None
            assert not self.is_ended
            action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Pull'
            body = {
                'n:Pull': {
                    'n:EnumerationContext': enumeration_context[0],
                    'n:MaxElements': str(max_elements),
                    }
                }
            response = self._perform_action(action, body)
            self.is_ended = 'n:EndOfSequence' in response['n:PullResponse']
            enumeration_context[0] = None if self.is_ended else response['n:PullResponse']['n:EnumerationContext']
            items = response['n:PullResponse']['n:Items']['w:Item']
            return _pick_objects(items)

        def _pick_objects(item_list):
            object_list = [item[self.cim_class.name] for item in item_list]
            for object in object_list:
                _logger.info('\tObject: %r', object)
            return object_list

        for item in _start():
            yield item
        while not enumeration_is_ended[0]:
            for item in _pull():
                yield item

    def get(self):
        _logger.info("Get %s where %r", self.cim_class, self.selectors)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Get'
        outcome = self._perform_action(action_url, {})
        instance = outcome[self.cim_class.name]
        return instance

    def put(self, new_properties_dict):
        _logger.info("Put %s where %r: %r", self.cim_class, self.selectors, new_properties_dict)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Put'
        body = {self.cim_class.name: new_properties_dict}
        body[self.cim_class.name]['@xmlns'] = self.cim_class.uri
        outcome = self._perform_action(action_url, body)
        instance = outcome[self.cim_class.name]
        return instance

    def create(self, properties_dict):
        assert not self.selectors
        _logger.info("Create %r: %r", self.cim_class, properties_dict)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Create'
        body = {self.cim_class.name: properties_dict}
        body[self.cim_class.name]['@xmlns'] = self.cim_class.uri
        outcome = self._perform_action(action_url, body)
        reference_parameters = outcome[u't:ResourceCreated'][u'a:ReferenceParameters']
        assert reference_parameters[u'w:ResourceURI'] == self.cim_class.uri
        selector_set = reference_parameters[u'w:SelectorSet']
        return selector_set

    def invoke_method(self, method_name, params, timeout_sec=None):
        _logger.info("Invoke %s.%s(%r) where %r", self.cim_class, method_name, params, self.selectors)
        action_uri = self.cim_class.method_uri(method_name)
        method_input = {'p:' + param_name: param_value for param_name, param_value in params.items()}
        method_input['@xmlns:p'] = self.cim_class.uri
        body = {method_name + '_INPUT': method_input}
        response = self._perform_action(action_uri, body, timeout_sec=timeout_sec)
        method_output = response[method_name + '_OUTPUT']
        if method_output[u'ReturnValue'] != u'0':
            error_code = int(method_output[u'ReturnValue'])
            raise RuntimeError('Non-zero return value 0x{:X} ({}) of {!s}.{!s}({!r}) where {!r}:\n{!s}'.format(
                error_code,
                _win32_error_codes.get(error_code, 'unknown'),
                self.cim_class.name,
                method_name,
                params,
                self.selectors,
                pformat(method_output),
                ))
        return method_output


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
