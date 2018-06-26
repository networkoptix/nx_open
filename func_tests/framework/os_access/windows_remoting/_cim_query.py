import logging
from pprint import pformat

import xmltodict
from winrm.exceptions import WinRMTransportError

_logger = logging.getLogger(__name__)


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


class _CimAction(object):
    common_namespaces = {
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
        CIMClass('Win32_FolderRedirectionHealth').uri: 'folder',  # Commonly encountered in responses.
        }

    def __init__(self, cim_class, action, selectors, body):
        self.cim_class = cim_class
        self.action = action
        self.selectors = selectors
        self.body = body
        self.namespaces = self.common_namespaces.copy()
        # noinspection PyTypeChecker
        self.namespaces[self.cim_class.uri] = None  # Desired class namespace is default.

    def _parent_is_wsman_items_element(self, parent_path, _key, _value):
        try:
            parent = parent_path[-1]
        except IndexError:
            return False
        parent_tag, _parent_attributes = parent
        return parent_tag == self.namespaces['http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd'] + ':Items'

    def _substitute_namespace_in_type_attribute(self, path, key, data):
        """Process namespaces in type attr value."""
        if key != '@' + self.namespaces['http://www.w3.org/2001/XMLSchema-instance'] + ':type':
            return key, data
        element_namespace_aliases = path[-1][1].get('xmlns', {})
        if element_namespace_aliases is None:
            return key, data
        old_namespace_alias, bare_data = data.split(':')
        new_namespace_alias = self.namespaces[element_namespace_aliases[old_namespace_alias]]
        if new_namespace_alias is None:
            return key, bare_data
        return key, new_namespace_alias + ':' + bare_data

    def perform(self, protocol, timeout_sec=None):
        # noinspection PyProtectedMember
        rq = {'env:Envelope': protocol._get_soap_header(resource_uri=self.cim_class.uri, action=self.action)}
        rq['env:Envelope'].setdefault('env:Body', self.body)
        rq['env:Envelope']['env:Header']['w:SelectorSet'] = {
            'w:Selector': [
                {'@Name': selector_name, '#text': selector_value}
                for selector_name, selector_value in self.selectors.items()
                ]
            }
        if timeout_sec is not None:
            rq['env:Envelope']['w:OperationTimeout'] = 'PT{}S'.format(timeout_sec),
        try:
            response = protocol.send_message(xmltodict.unparse(rq))
        except WinRMTransportError as e:
            _logger.exception("XML:\n%s", e.response_text)
            raise

        response_dict = xmltodict.parse(
            response,
            dict_constructor=dict,  # Order is meaningless.
            process_namespaces=True, namespaces=self.namespaces,  # Force namespace aliases.
            force_list=self._parent_is_wsman_items_element,
            postprocessor=self._substitute_namespace_in_type_attribute,
            )
        return response_dict['env:Envelope']['env:Body']


class _Enumeration(object):
    def __init__(self, protocol, wmi_class, selectors, max_elements=32000):
        self.protocol = protocol
        self.cim_class = wmi_class
        self.selectors = selectors
        self.max_elements = max_elements
        self.enumeration_context = None
        self.is_ended = False

    def _start(self):
        _logger.debug("Start enumerating %s where %r", self.cim_class, self.selectors)
        assert not self.is_ended
        action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate'
        body = {
            'n:Enumerate': {
                'w:OptimizeEnumeration': None,
                'w:MaxElements': str(self.max_elements),
                # See: https://www.dmtf.org/sites/default/files/standards/documents/DSP0226_1.0.0.pdf, 8.7.
                'w:EnumerationMode': 'EnumerateObjectAndEPR',
                }
            }
        response = _CimAction(self.cim_class, action, self.selectors, body).perform(self.protocol)
        self.enumeration_context = response['n:EnumerateResponse']['n:EnumerationContext']
        self.is_ended = 'w:EndOfSequence' in response['n:EnumerateResponse']
        items = response['n:EnumerateResponse']['w:Items']['w:Item']
        objects = [item[self.cim_class.name] for item in items]
        return objects

    def _pull(self):
        _logger.debug("Continue enumerating %s where %r", self.cim_class, self.selectors)
        assert self.enumeration_context is not None
        assert not self.is_ended
        action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Pull'
        body = {
            'n:Pull': {
                'n:EnumerationContext': self.enumeration_context,
                'n:MaxElements': str(self.max_elements),
                }
            }
        response = _CimAction(self.cim_class, action, self.selectors, body).perform(self.protocol)
        self.is_ended = 'n:EndOfSequence' in response['n:PullResponse']
        self.enumeration_context = None if self.is_ended else response['n:PullResponse']['n:EnumerationContext']
        items = response['n:PullResponse']['n:Items']['w:Item']
        objects = [item[self.cim_class.name] for item in items]
        return objects

    def enumerate(self):
        for item in self._start():
            yield item
        while not self.is_ended:
            for item in self._pull():
                yield item


class CIMQuery(object):
    def __init__(self, protocol, cim_class, selectors):
        self.protocol = protocol
        self.cim_class = cim_class
        self.selectors = selectors

    def enumerate(self, max_elements=32000):
        _logger.debug("Enumerate %s where %r", self.cim_class, self.selectors)
        return _Enumeration(self.protocol, self.cim_class, self.selectors, max_elements=max_elements).enumerate()

    def get(self):
        _logger.debug("Get %s where %r", self.cim_class, self.selectors)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Get'
        action = _CimAction(self.cim_class, action_url, self.selectors, {})
        outcome = action.perform(self.protocol)
        instance = outcome[self.cim_class.name]
        return instance

    def put(self, new_properties_dict):
        _logger.debug("Put %s where %r: %r", self.cim_class, self.selectors, new_properties_dict)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Put'
        body = {self.cim_class.name: new_properties_dict}
        body[self.cim_class.name]['@xmlns'] = self.cim_class.uri
        action = _CimAction(self.cim_class, action_url, self.selectors, body)
        outcome = action.perform(self.protocol)
        instance = outcome[self.cim_class.name]
        return instance

    def create(self, properties_dict):
        assert not self.selectors
        _logger.debug("Create %r: %r", self.cim_class, properties_dict)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Create'
        body = {self.cim_class.name: properties_dict}
        body[self.cim_class.name]['@xmlns'] = self.cim_class.uri
        action = _CimAction(self.cim_class, action_url, self.selectors, body)
        outcome = action.perform(self.protocol)
        reference_parameters = outcome[u't:ResourceCreated'][u'a:ReferenceParameters']
        assert reference_parameters[u'w:ResourceURI'] == self.cim_class.uri
        selector_set = reference_parameters[u'w:SelectorSet']
        return selector_set

    def invoke_method(self, method_name, params, timeout_sec=None):
        _logger.debug("Invoke %s.%s(%r) where %r", self.cim_class, method_name, params, self.selectors)
        action_uri = self.cim_class.method_uri(method_name)
        method_input = {'p:' + param_name: param_value for param_name, param_value in params.items()}
        method_input['@xmlns:p'] = self.cim_class.uri
        body = {method_name + '_INPUT': method_input}
        action = _CimAction(self.cim_class, action_uri, self.selectors, body)
        response = action.perform(self.protocol, timeout_sec=timeout_sec)
        method_output = response[method_name + '_OUTPUT']
        if method_output[u'ReturnValue'] != u'0':
            raise RuntimeError('Non-zero return value of {!s}.{!s}({!r}) where {!r}:\n{!s}'.format(
                self.cim_class.name, method_name, params, self.selectors, pformat(method_output)))
        return method_output
