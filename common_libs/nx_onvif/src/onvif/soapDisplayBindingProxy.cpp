/* soapDisplayBindingProxy.cpp
   Generated by gSOAP 2.8.66 for ../result/interim/onvif.h_

gSOAP XML Web services tools
Copyright (C) 2000-2018, Robert van Engelen, Genivia Inc. All Rights Reserved.
The soapcpp2 tool and its generated software are released under the GPL.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
--------------------------------------------------------------------------------
A commercial use license is available from Genivia Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

#include "soapDisplayBindingProxy.h"

DisplayBindingProxy::DisplayBindingProxy()
{	this->soap = soap_new();
	this->soap_own = true;
	DisplayBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

DisplayBindingProxy::DisplayBindingProxy(const DisplayBindingProxy& rhs)
{	this->soap = rhs.soap;
	this->soap_own = false;
	this->soap_endpoint = rhs.soap_endpoint;
}

DisplayBindingProxy::DisplayBindingProxy(struct soap *_soap)
{	this->soap = _soap;
	this->soap_own = false;
	DisplayBindingProxy_init(_soap->imode, _soap->omode);
}

DisplayBindingProxy::DisplayBindingProxy(const char *endpoint)
{	this->soap = soap_new();
	this->soap_own = true;
	DisplayBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
	soap_endpoint = endpoint;
}

DisplayBindingProxy::DisplayBindingProxy(soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	DisplayBindingProxy_init(iomode, iomode);
}

DisplayBindingProxy::DisplayBindingProxy(const char *endpoint, soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	DisplayBindingProxy_init(iomode, iomode);
	soap_endpoint = endpoint;
}

DisplayBindingProxy::DisplayBindingProxy(soap_mode imode, soap_mode omode)
{	this->soap = soap_new();
	this->soap_own = true;
	DisplayBindingProxy_init(imode, omode);
}

DisplayBindingProxy::~DisplayBindingProxy()
{	if (this->soap_own)
		soap_free(this->soap);
}

void DisplayBindingProxy::DisplayBindingProxy_init(soap_mode imode, soap_mode omode)
{	soap_imode(this->soap, imode);
	soap_omode(this->soap, omode);
	soap_endpoint = NULL;
	static const struct Namespace namespaces[] = {
        {"SOAP-ENV", "http://www.w3.org/2003/05/soap-envelope", "http://schemas.xmlsoap.org/soap/envelope/", NULL},
        {"SOAP-ENC", "http://www.w3.org/2003/05/soap-encoding", "http://schemas.xmlsoap.org/soap/encoding/", NULL},
        {"xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL},
        {"xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL},
        {"chan", "http://schemas.microsoft.com/ws/2005/02/duplex", NULL, NULL},
        {"wsa5", "http://www.w3.org/2005/08/addressing", "http://schemas.xmlsoap.org/ws/2004/08/addressing", NULL},
        {"wsdd", "http://schemas.xmlsoap.org/ws/2005/04/discovery", NULL, NULL},
        {"c14n", "http://www.w3.org/2001/10/xml-exc-c14n#", NULL, NULL},
        {"ds", "http://www.w3.org/2000/09/xmldsig#", NULL, NULL},
        {"saml1", "urn:oasis:names:tc:SAML:1.0:assertion", NULL, NULL},
        {"saml2", "urn:oasis:names:tc:SAML:2.0:assertion", NULL, NULL},
        {"wsu", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd", NULL, NULL},
        {"xenc", "http://www.w3.org/2001/04/xmlenc#", NULL, NULL},
        {"wsc", "http://docs.oasis-open.org/ws-sx/ws-secureconversation/200512", NULL, NULL},
        {"wsse", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd", "http://docs.oasis-open.org/wss/oasis-wss-wssecurity-secext-1.1.xsd", NULL},
        {"onvifPacs", "http://www.onvif.org/ver10/pacs", NULL, NULL},
        {"xmime", "http://tempuri.org/xmime.xsd", NULL, NULL},
        {"xop", "http://www.w3.org/2004/08/xop/include", NULL, NULL},
        {"onvifXsd", "http://www.onvif.org/ver10/schema", NULL, NULL},
        {"oasisWsrf", "http://docs.oasis-open.org/wsrf/bf-2", NULL, NULL},
        {"oasisWsnT1", "http://docs.oasis-open.org/wsn/t-1", NULL, NULL},
        {"oasisWsrfR2", "http://docs.oasis-open.org/wsrf/r-2", NULL, NULL},
        {"onvifAccessControl", "http://www.onvif.org/ver10/accesscontrol/wsdl", NULL, NULL},
        {"onvifAccessRules", "http://www.onvif.org/ver10/accessrules/wsdl", NULL, NULL},
        {"onvifActionEngine", "http://www.onvif.org/ver10/actionengine/wsdl", NULL, NULL},
        {"onvifAdvancedSecurity-assb", "http://www.onvif.org/ver10/advancedsecurity/wsdl/AdvancedSecurityServiceBinding", NULL, NULL},
        {"onvifAdvancedSecurity-db", "http://www.onvif.org/ver10/advancedsecurity/wsdl/Dot1XBinding", NULL, NULL},
        {"onvifAdvancedSecurity-kb", "http://www.onvif.org/ver10/advancedsecurity/wsdl/KeystoreBinding", NULL, NULL},
        {"onvifAdvancedSecurity-tsb", "http://www.onvif.org/ver10/advancedsecurity/wsdl/TLSServerBinding", NULL, NULL},
        {"onvifAdvancedSecurity", "http://www.onvif.org/ver10/advancedsecurity/wsdl", NULL, NULL},
        {"onvifAnalytics-aeb", "http://www.onvif.org/ver20/analytics/wsdl/AnalyticsEngineBinding", NULL, NULL},
        {"onvifAnalytics-reb", "http://www.onvif.org/ver20/analytics/wsdl/RuleEngineBinding", NULL, NULL},
        {"onvifAnalytics", "http://www.onvif.org/ver20/analytics/wsdl", NULL, NULL},
        {"onvifAnalyticsDevice", "http://www.onvif.org/ver10/analyticsdevice/wsdl", NULL, NULL},
        {"onvifCredential", "http://www.onvif.org/ver10/credential/wsdl", NULL, NULL},
        {"onvifDevice", "http://www.onvif.org/ver10/device/wsdl", NULL, NULL},
        {"onvifDeviceIO", "http://www.onvif.org/ver10/deviceIO/wsdl", NULL, NULL},
        {"onvifDisplay", "http://www.onvif.org/ver10/display/wsdl", NULL, NULL},
        {"onvifDoorControl", "http://www.onvif.org/ver10/doorcontrol/wsdl", NULL, NULL},
        {"onvifEvents-cpb", "http://www.onvif.org/ver10/events/wsdl/CreatePullPointBinding", NULL, NULL},
        {"onvifEvents-eb", "http://www.onvif.org/ver10/events/wsdl/EventBinding", NULL, NULL},
        {"onvifEvents-ncb", "http://www.onvif.org/ver10/events/wsdl/NotificationConsumerBinding", NULL, NULL},
        {"onvifEvents-npb", "http://www.onvif.org/ver10/events/wsdl/NotificationProducerBinding", NULL, NULL},
        {"onvifEvents-ppb", "http://www.onvif.org/ver10/events/wsdl/PullPointBinding", NULL, NULL},
        {"onvifEvents", "http://www.onvif.org/ver10/events/wsdl", NULL, NULL},
        {"onvifEvents-pps", "http://www.onvif.org/ver10/events/wsdl/PullPointSubscriptionBinding", NULL, NULL},
        {"onvifEvents-psmb", "http://www.onvif.org/ver10/events/wsdl/PausableSubscriptionManagerBinding", NULL, NULL},
        {"oasisWsnB2", "http://docs.oasis-open.org/wsn/b-2", NULL, NULL},
        {"onvifEvents-smb", "http://www.onvif.org/ver10/events/wsdl/SubscriptionManagerBinding", NULL, NULL},
        {"onvifImg", "http://www.onvif.org/ver20/imaging/wsdl", NULL, NULL},
        {"onvifMedia", "http://www.onvif.org/ver10/media/wsdl", NULL, NULL},
        {"onvifMedia2", "http://www.onvif.org/ver20/media/wsdl", NULL, NULL},
        {"onvifNetwork-dlb", "http://www.onvif.org/ver10/network/wsdl/DiscoveryLookupBinding", NULL, NULL},
        {"onvifNetwork-rdb", "http://www.onvif.org/ver10/network/wsdl/RemoteDiscoveryBinding", NULL, NULL},
        {"onvifNetwork", "http://www.onvif.org/ver10/network/wsdl", NULL, NULL},
        {"onvifProvisioning", "http://www.onvif.org/ver10/provisioning/wsdl", NULL, NULL},
        {"onvifPtz", "http://www.onvif.org/ver20/ptz/wsdl", NULL, NULL},
        {"onvifReceiver", "http://www.onvif.org/ver10/receiver/wsdl", NULL, NULL},
        {"onvifRecording", "http://www.onvif.org/ver10/recording/wsdl", NULL, NULL},
        {"onvifReplay", "http://www.onvif.org/ver10/replay/wsdl", NULL, NULL},
        {"onvifScedule", "http://www.onvif.org/ver10/schedule/wsdl", NULL, NULL},
        {"onvifSearch", "http://www.onvif.org/ver10/search/wsdl", NULL, NULL},
        {"onvifThermal", "http://www.onvif.org/ver10/thermal/wsdl", NULL, NULL},
        {NULL, NULL, NULL, NULL}
    };
	soap_set_namespaces(this->soap, namespaces);
}

DisplayBindingProxy *DisplayBindingProxy::copy()
{	DisplayBindingProxy *dup = SOAP_NEW_UNMANAGED(DisplayBindingProxy);
	if (dup)
	{	soap_done(dup->soap);
		soap_copy_context(dup->soap, this->soap);
	}
	return dup;
}

DisplayBindingProxy& DisplayBindingProxy::operator=(const DisplayBindingProxy& rhs)
{	if (this->soap != rhs.soap)
	{	if (this->soap_own)
			soap_free(this->soap);
		this->soap = rhs.soap;
		this->soap_own = false;
		this->soap_endpoint = rhs.soap_endpoint;
	}
	return *this;
}

void DisplayBindingProxy::destroy()
{	soap_destroy(this->soap);
	soap_end(this->soap);
}

void DisplayBindingProxy::reset()
{	this->destroy();
	soap_done(this->soap);
	soap_initialize(this->soap);
	DisplayBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

void DisplayBindingProxy::soap_noheader()
{	this->soap->header = NULL;
}

void DisplayBindingProxy::soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID)
{	::soap_header(this->soap);
	this->soap->header->wsa5__MessageID = wsa5__MessageID;
	this->soap->header->wsa5__RelatesTo = wsa5__RelatesTo;
	this->soap->header->wsa5__From = wsa5__From;
	this->soap->header->wsa5__ReplyTo = wsa5__ReplyTo;
	this->soap->header->wsa5__FaultTo = wsa5__FaultTo;
	this->soap->header->wsa5__To = wsa5__To;
	this->soap->header->wsa5__Action = wsa5__Action;
	this->soap->header->chan__ChannelInstance = chan__ChannelInstance;
	this->soap->header->wsdd__AppSequence = wsdd__AppSequence;
	this->soap->header->wsse__Security = wsse__Security;
	this->soap->header->subscriptionID = subscriptionID;
}

::SOAP_ENV__Header *DisplayBindingProxy::soap_header()
{	return this->soap->header;
}

::SOAP_ENV__Fault *DisplayBindingProxy::soap_fault()
{	return this->soap->fault;
}

const char *DisplayBindingProxy::soap_fault_string()
{	return *soap_faultstring(this->soap);
}

const char *DisplayBindingProxy::soap_fault_detail()
{	return *soap_faultdetail(this->soap);
}

int DisplayBindingProxy::soap_close_socket()
{	return soap_closesock(this->soap);
}

int DisplayBindingProxy::soap_force_close_socket()
{	return soap_force_closesock(this->soap);
}

void DisplayBindingProxy::soap_print_fault(FILE *fd)
{	::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void DisplayBindingProxy::soap_stream_fault(std::ostream& os)
{	::soap_stream_fault(this->soap, os);
}
#endif

char *DisplayBindingProxy::soap_sprint_fault(char *buf, size_t len)
{	return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

int DisplayBindingProxy::GetServiceCapabilities(const char *endpoint, const char *soap_action, _onvifDisplay__GetServiceCapabilities *onvifDisplay__GetServiceCapabilities, _onvifDisplay__GetServiceCapabilitiesResponse &onvifDisplay__GetServiceCapabilitiesResponse)
{
	struct __onvifDisplay__GetServiceCapabilities soap_tmp___onvifDisplay__GetServiceCapabilities;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/GetServiceCapabilities";
	soap_tmp___onvifDisplay__GetServiceCapabilities.onvifDisplay__GetServiceCapabilities = onvifDisplay__GetServiceCapabilities;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__GetServiceCapabilities(soap, &soap_tmp___onvifDisplay__GetServiceCapabilities);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__GetServiceCapabilities(soap, &soap_tmp___onvifDisplay__GetServiceCapabilities, "-onvifDisplay:GetServiceCapabilities", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__GetServiceCapabilities(soap, &soap_tmp___onvifDisplay__GetServiceCapabilities, "-onvifDisplay:GetServiceCapabilities", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__GetServiceCapabilitiesResponse*>(&onvifDisplay__GetServiceCapabilitiesResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__GetServiceCapabilitiesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__GetServiceCapabilitiesResponse.soap_get(soap, "onvifDisplay:GetServiceCapabilitiesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::GetLayout(const char *endpoint, const char *soap_action, _onvifDisplay__GetLayout *onvifDisplay__GetLayout, _onvifDisplay__GetLayoutResponse &onvifDisplay__GetLayoutResponse)
{
	struct __onvifDisplay__GetLayout soap_tmp___onvifDisplay__GetLayout;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/GetLayout";
	soap_tmp___onvifDisplay__GetLayout.onvifDisplay__GetLayout = onvifDisplay__GetLayout;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__GetLayout(soap, &soap_tmp___onvifDisplay__GetLayout);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__GetLayout(soap, &soap_tmp___onvifDisplay__GetLayout, "-onvifDisplay:GetLayout", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__GetLayout(soap, &soap_tmp___onvifDisplay__GetLayout, "-onvifDisplay:GetLayout", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__GetLayoutResponse*>(&onvifDisplay__GetLayoutResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__GetLayoutResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__GetLayoutResponse.soap_get(soap, "onvifDisplay:GetLayoutResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::SetLayout(const char *endpoint, const char *soap_action, _onvifDisplay__SetLayout *onvifDisplay__SetLayout, _onvifDisplay__SetLayoutResponse &onvifDisplay__SetLayoutResponse)
{
	struct __onvifDisplay__SetLayout soap_tmp___onvifDisplay__SetLayout;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/SetLayout";
	soap_tmp___onvifDisplay__SetLayout.onvifDisplay__SetLayout = onvifDisplay__SetLayout;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__SetLayout(soap, &soap_tmp___onvifDisplay__SetLayout);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__SetLayout(soap, &soap_tmp___onvifDisplay__SetLayout, "-onvifDisplay:SetLayout", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__SetLayout(soap, &soap_tmp___onvifDisplay__SetLayout, "-onvifDisplay:SetLayout", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__SetLayoutResponse*>(&onvifDisplay__SetLayoutResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__SetLayoutResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__SetLayoutResponse.soap_get(soap, "onvifDisplay:SetLayoutResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::GetDisplayOptions(const char *endpoint, const char *soap_action, _onvifDisplay__GetDisplayOptions *onvifDisplay__GetDisplayOptions, _onvifDisplay__GetDisplayOptionsResponse &onvifDisplay__GetDisplayOptionsResponse)
{
	struct __onvifDisplay__GetDisplayOptions soap_tmp___onvifDisplay__GetDisplayOptions;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/GetDisplayOptions";
	soap_tmp___onvifDisplay__GetDisplayOptions.onvifDisplay__GetDisplayOptions = onvifDisplay__GetDisplayOptions;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__GetDisplayOptions(soap, &soap_tmp___onvifDisplay__GetDisplayOptions);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__GetDisplayOptions(soap, &soap_tmp___onvifDisplay__GetDisplayOptions, "-onvifDisplay:GetDisplayOptions", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__GetDisplayOptions(soap, &soap_tmp___onvifDisplay__GetDisplayOptions, "-onvifDisplay:GetDisplayOptions", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__GetDisplayOptionsResponse*>(&onvifDisplay__GetDisplayOptionsResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__GetDisplayOptionsResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__GetDisplayOptionsResponse.soap_get(soap, "onvifDisplay:GetDisplayOptionsResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::GetPaneConfigurations(const char *endpoint, const char *soap_action, _onvifDisplay__GetPaneConfigurations *onvifDisplay__GetPaneConfigurations, _onvifDisplay__GetPaneConfigurationsResponse &onvifDisplay__GetPaneConfigurationsResponse)
{
	struct __onvifDisplay__GetPaneConfigurations soap_tmp___onvifDisplay__GetPaneConfigurations;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/GetPaneConfigurations";
	soap_tmp___onvifDisplay__GetPaneConfigurations.onvifDisplay__GetPaneConfigurations = onvifDisplay__GetPaneConfigurations;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__GetPaneConfigurations(soap, &soap_tmp___onvifDisplay__GetPaneConfigurations);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__GetPaneConfigurations(soap, &soap_tmp___onvifDisplay__GetPaneConfigurations, "-onvifDisplay:GetPaneConfigurations", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__GetPaneConfigurations(soap, &soap_tmp___onvifDisplay__GetPaneConfigurations, "-onvifDisplay:GetPaneConfigurations", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__GetPaneConfigurationsResponse*>(&onvifDisplay__GetPaneConfigurationsResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__GetPaneConfigurationsResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__GetPaneConfigurationsResponse.soap_get(soap, "onvifDisplay:GetPaneConfigurationsResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::GetPaneConfiguration(const char *endpoint, const char *soap_action, _onvifDisplay__GetPaneConfiguration *onvifDisplay__GetPaneConfiguration, _onvifDisplay__GetPaneConfigurationResponse &onvifDisplay__GetPaneConfigurationResponse)
{
	struct __onvifDisplay__GetPaneConfiguration soap_tmp___onvifDisplay__GetPaneConfiguration;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/GetPaneConfiguration";
	soap_tmp___onvifDisplay__GetPaneConfiguration.onvifDisplay__GetPaneConfiguration = onvifDisplay__GetPaneConfiguration;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__GetPaneConfiguration(soap, &soap_tmp___onvifDisplay__GetPaneConfiguration);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__GetPaneConfiguration(soap, &soap_tmp___onvifDisplay__GetPaneConfiguration, "-onvifDisplay:GetPaneConfiguration", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__GetPaneConfiguration(soap, &soap_tmp___onvifDisplay__GetPaneConfiguration, "-onvifDisplay:GetPaneConfiguration", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__GetPaneConfigurationResponse*>(&onvifDisplay__GetPaneConfigurationResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__GetPaneConfigurationResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__GetPaneConfigurationResponse.soap_get(soap, "onvifDisplay:GetPaneConfigurationResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::SetPaneConfigurations(const char *endpoint, const char *soap_action, _onvifDisplay__SetPaneConfigurations *onvifDisplay__SetPaneConfigurations, _onvifDisplay__SetPaneConfigurationsResponse &onvifDisplay__SetPaneConfigurationsResponse)
{
	struct __onvifDisplay__SetPaneConfigurations soap_tmp___onvifDisplay__SetPaneConfigurations;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/SetPaneConfigurations";
	soap_tmp___onvifDisplay__SetPaneConfigurations.onvifDisplay__SetPaneConfigurations = onvifDisplay__SetPaneConfigurations;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__SetPaneConfigurations(soap, &soap_tmp___onvifDisplay__SetPaneConfigurations);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__SetPaneConfigurations(soap, &soap_tmp___onvifDisplay__SetPaneConfigurations, "-onvifDisplay:SetPaneConfigurations", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__SetPaneConfigurations(soap, &soap_tmp___onvifDisplay__SetPaneConfigurations, "-onvifDisplay:SetPaneConfigurations", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__SetPaneConfigurationsResponse*>(&onvifDisplay__SetPaneConfigurationsResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__SetPaneConfigurationsResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__SetPaneConfigurationsResponse.soap_get(soap, "onvifDisplay:SetPaneConfigurationsResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::SetPaneConfiguration(const char *endpoint, const char *soap_action, _onvifDisplay__SetPaneConfiguration *onvifDisplay__SetPaneConfiguration, _onvifDisplay__SetPaneConfigurationResponse &onvifDisplay__SetPaneConfigurationResponse)
{
	struct __onvifDisplay__SetPaneConfiguration soap_tmp___onvifDisplay__SetPaneConfiguration;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/SetPaneConfiguration";
	soap_tmp___onvifDisplay__SetPaneConfiguration.onvifDisplay__SetPaneConfiguration = onvifDisplay__SetPaneConfiguration;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__SetPaneConfiguration(soap, &soap_tmp___onvifDisplay__SetPaneConfiguration);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__SetPaneConfiguration(soap, &soap_tmp___onvifDisplay__SetPaneConfiguration, "-onvifDisplay:SetPaneConfiguration", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__SetPaneConfiguration(soap, &soap_tmp___onvifDisplay__SetPaneConfiguration, "-onvifDisplay:SetPaneConfiguration", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__SetPaneConfigurationResponse*>(&onvifDisplay__SetPaneConfigurationResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__SetPaneConfigurationResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__SetPaneConfigurationResponse.soap_get(soap, "onvifDisplay:SetPaneConfigurationResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::CreatePaneConfiguration(const char *endpoint, const char *soap_action, _onvifDisplay__CreatePaneConfiguration *onvifDisplay__CreatePaneConfiguration, _onvifDisplay__CreatePaneConfigurationResponse &onvifDisplay__CreatePaneConfigurationResponse)
{
	struct __onvifDisplay__CreatePaneConfiguration soap_tmp___onvifDisplay__CreatePaneConfiguration;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/CreatePaneConfiguration";
	soap_tmp___onvifDisplay__CreatePaneConfiguration.onvifDisplay__CreatePaneConfiguration = onvifDisplay__CreatePaneConfiguration;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__CreatePaneConfiguration(soap, &soap_tmp___onvifDisplay__CreatePaneConfiguration);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__CreatePaneConfiguration(soap, &soap_tmp___onvifDisplay__CreatePaneConfiguration, "-onvifDisplay:CreatePaneConfiguration", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__CreatePaneConfiguration(soap, &soap_tmp___onvifDisplay__CreatePaneConfiguration, "-onvifDisplay:CreatePaneConfiguration", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__CreatePaneConfigurationResponse*>(&onvifDisplay__CreatePaneConfigurationResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__CreatePaneConfigurationResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__CreatePaneConfigurationResponse.soap_get(soap, "onvifDisplay:CreatePaneConfigurationResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int DisplayBindingProxy::DeletePaneConfiguration(const char *endpoint, const char *soap_action, _onvifDisplay__DeletePaneConfiguration *onvifDisplay__DeletePaneConfiguration, _onvifDisplay__DeletePaneConfigurationResponse &onvifDisplay__DeletePaneConfigurationResponse)
{
	struct __onvifDisplay__DeletePaneConfiguration soap_tmp___onvifDisplay__DeletePaneConfiguration;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver10/display/wsdl/DeletePaneConfiguration";
	soap_tmp___onvifDisplay__DeletePaneConfiguration.onvifDisplay__DeletePaneConfiguration = onvifDisplay__DeletePaneConfiguration;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifDisplay__DeletePaneConfiguration(soap, &soap_tmp___onvifDisplay__DeletePaneConfiguration);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifDisplay__DeletePaneConfiguration(soap, &soap_tmp___onvifDisplay__DeletePaneConfiguration, "-onvifDisplay:DeletePaneConfiguration", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifDisplay__DeletePaneConfiguration(soap, &soap_tmp___onvifDisplay__DeletePaneConfiguration, "-onvifDisplay:DeletePaneConfiguration", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifDisplay__DeletePaneConfigurationResponse*>(&onvifDisplay__DeletePaneConfigurationResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifDisplay__DeletePaneConfigurationResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifDisplay__DeletePaneConfigurationResponse.soap_get(soap, "onvifDisplay:DeletePaneConfigurationResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}
/* End of client proxy code */
