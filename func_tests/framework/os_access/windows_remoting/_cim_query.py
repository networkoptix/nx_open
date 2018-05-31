import logging
from pprint import pformat

import xmltodict
from winrm.exceptions import WinRMTransportError

_logger = logging.getLogger(__name__)


class _CimAction(object):
    cim_directory = 'http://schemas.microsoft.com/wbem/wsman/1/wmi/root/cimv2/'

    common_namespaces = {
        'http://www.w3.org/XML/1998/namespace': 'xml',
        'http://www.w3.org/2003/05/soap-envelope': 'env',
        'http://schemas.xmlsoap.org/ws/2004/09/enumeration': 'n',
        'http://www.w3.org/2001/XMLSchema-instance': 'xsi',
        'http://www.w3.org/2001/XMLSchema': 'xs',
        'http://schemas.dmtf.org/wbem/wscim/1/common': 'cim',
        'http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd': 'w',
        cim_directory + 'Win32_FolderRedirectionHealth': 'folder',  # Commonly encountered in responses.
        }

    def __init__(self, resource_uri, action, selectors, body):
        self.resource_uri = resource_uri
        self.action = action
        self.selectors = selectors
        self.body = body
        self.namespaces = self.common_namespaces.copy()
        self.namespaces[self.resource_uri] = None  # Desired class namespace is default.

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

    def perform(self, protocol):
        # noinspection PyProtectedMember
        rq = {'env:Envelope': protocol._get_soap_header(resource_uri=self.resource_uri, action=self.action)}
        rq['env:Envelope'].setdefault('env:Body', self.body)
        rq['env:Envelope']['env:Header']['w:SelectorSet'] = {
            'w:Selector': [
                {'@Name': selector_name, '#text': selector_value}
                for selector_name, selector_value in self.selectors.items()
                ]
            }
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
    def __init__(self, protocol, class_name, selectors, max_elements=32000):
        self.protocol = protocol
        self.class_name = class_name
        self.selectors = selectors
        self.max_elements = max_elements
        self.enumeration_context = None
        self.is_ended = False

    def _start(self):
        _logger.debug("Start enumerating %s where %r", self.class_name, self.selectors)
        assert not self.is_ended
        action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate'
        resource_uri = _CimAction.cim_directory + self.class_name
        body = {
            'n:Enumerate': {
                'w:OptimizeEnumeration': None,
                'w:MaxElements': str(self.max_elements),
                }
            }
        response = _CimAction(resource_uri, action, self.selectors, body).perform(self.protocol)
        self.enumeration_context = response['n:EnumerateResponse']['n:EnumerationContext']
        self.is_ended = any((
            'n:EndOfSequence' in response['n:EnumerateResponse'],
            'w:EndOfSequence' in response['n:EnumerateResponse']))
        items = response['n:EnumerateResponse']['w:Items'][self.class_name]
        assert isinstance(items, list)
        return items if isinstance(items, list) else [items]

    def _pull(self):
        _logger.debug("Continue enumerating %s where %r", self.class_name, self.selectors)
        assert self.enumeration_context is not None
        assert not self.is_ended
        action = 'http://schemas.xmlsoap.org/ws/2004/09/enumeration/Pull'
        resource_uri = _CimAction.cim_directory + self.class_name
        body = {
            'n:Pull': {
                'n:EnumerationContext': self.enumeration_context,
                'n:MaxElements': str(self.max_elements),
                }
            }
        response = _CimAction(resource_uri, action, self.selectors, body).perform(self.protocol)
        self.is_ended = any((
            'n:EndOfSequence' in response['n:PullResponse'],
            'w:EndOfSequence' in response['n:PullResponse']))
        items = response['n:PullResponse']['n:Items'][self.class_name]
        assert isinstance(items, list)
        return items if isinstance(items, list) else [items]

    def enumerate(self):
        for item in self._start():
            yield item
        while not self.is_ended:
            for item in self._pull():
                yield item


class CIMQuery(object):
    def __init__(self, protocol, class_name, selectors):
        self.protocol = protocol
        self.class_name = class_name
        self.selectors = selectors

    def enumerate(self, max_elements=32000):
        _logger.debug("Enumerate %s where %r", self.class_name, self.selectors)
        return _Enumeration(self.protocol, self.class_name, self.selectors, max_elements=max_elements).enumerate()

    def get_one(self):
        _logger.debug("Get %s where %r", self.class_name, self.selectors)
        action_url = 'http://schemas.xmlsoap.org/ws/2004/09/transfer/Get'
        resource_uri = _CimAction.cim_directory + self.class_name
        action = _CimAction(resource_uri, action_url, self.selectors, {})
        outcome = action.perform(self.protocol)
        instance = outcome[self.class_name]
        return instance

    def invoke_method(self, method_name, params):
        _logger.debug("Invoke %s.%s(%r) where %r", self.class_name, method_name, params, self.selectors)
        resource_uri = _CimAction.cim_directory + self.class_name
        action = resource_uri + '/' + method_name
        method_input = {'p:' + param_name: param_value for param_name, param_value in params.items()}
        method_input['@xmlns:p'] = resource_uri
        body = {method_name + '_INPUT': method_input}
        response = _CimAction(resource_uri, action, self.selectors, body).perform(self.protocol)
        method_output = response[method_name + '_OUTPUT']
        if method_output[u'ReturnValue'] != u'0':
            raise RuntimeError('Non-zero return value of {!s}.{!s}({!r}) where {!r}:\n{!s}'.format(
                self.class_name, method_name, params, self.selectors, pformat(method_output)))
        return method_output
